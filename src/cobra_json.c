/*
 * This file is part of the public release of Cobra. It is subject to the
 * terms in the License file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com/cobra
 */

#include "cobra_pre.h"

int	 json_format;
int	 json_plus;
int	 nr_json;
int	 p_matched;
int	 stream;
char	 bvars[512];
char	 json_msg[512];
char	*glob_te = ""; // optional type field in json output
char	*scrub_caption = "";
char	*cobra_texpr;		// -e xxx
Match	*matches;
Match	*old_matches;
Match	*free_match;
Named	*namedset;
Bound	*free_bound;

extern Prim	*prim;
extern FILE	*track_fd;

static void
cleaned_up(FILE *fd, const char *tp)
{	const char *p = tp;

	// intercept especially " and things like \n
	// the argument tp is always quoted

	while (p && *p != '\0')
	{	switch (*p) {
		case '\\':
			fprintf(fd, " ");
			break;
		case '\"':
		case '\'':
		case ':':
			fprintf(fd, "\\");
			// fall thru
		default:
			fprintf(fd, "%c", *p);
			break;
		}
		p++;
	}
}

static void
check_bvar(Prim *c)
{	char buf[512];

	if ((c->mark & (4|16)) == 0
	||   strlen(c->txt) >= sizeof(buf)-1)
	{	return;
	}

	if (c->mark & 4)	// matched
	{	sprintf(buf, "->%d", c->lnr);
	} else 			// defined
	{	sprintf(buf, "%s %d", c->txt, c->lnr);
	}
	if (strstr(bvars, buf))
	{	return;
	}
	if (strlen(c->txt) + strlen(bvars) + 3 < sizeof(bvars))
	{	if (bvars[0] != '\0'
		&&  (c->mark&16))
		{	strcat(bvars, ", ");	// check_bvar
		}
		strcat(bvars, buf);
	}
}

void
clr_matches(int which)
{	Match *m, *nm;
	Bound *b, *nb;

	m = (which == NEW_M) ? matches : old_matches;

	for (; m; m = nm)
	{	nm = m->nxt;
		m->from = (Prim *) 0;
		m->upto = (Prim *) 0;
		m->nxt = free_match;
		free_match = m;
		for (b = m->bind; b; b = nb)
		{	nb = b->nxt;
			b->bdef = (Prim *) 0;
			b->ref  = (Prim *) 0;
			b->nxt  = free_bound;
			free_bound = b;
	}	}

	if (which == NEW_M)
	{	matches     = (Match *) 0;
	} else
	{	old_matches = (Match *) 0;
	}
}

void
clear_matches(void)
{
	clr_matches(NEW_M);
}

Match *
findset(const char *s, int complain, int who)
{	Named *x;
	char *p;
	int n = 0;

	if ((p = strchr(s, ':')) != NULL)
	{	*p = '\0';
	}

	for (x = namedset; x; x = x->nxt)
	{	n++;
		if (strcmp(x->nm, s) == 0)
		{	return x->m;
	}	}

	if (complain && !json_format && !no_display && !no_match)
	{	fprintf(stderr, "named set '%s' not found\n", s);
		fprintf(stderr, "there are %d stored sets: ", n);
		for (x = namedset; x; x = x->nxt)
		{	fprintf(stderr, "'%s', ", x->nm);
		}
		fprintf(stderr, "\n");
	}

	return (Match *) 0;
}

void
add_match(Prim *f, Prim *t, Store *bd)
{	Match *m;
	Store *b;
	Bound *n;
	FILE *fd = (track_fd) ? track_fd : stdout;

	if (free_match)
	{	m = free_match;
		free_match = m->nxt;
		m->bind = (Bound *) 0;
	} else
	{	m = (Match *) emalloc(sizeof(Match), 105);
	}
	m->from = f;
	m->upto = t;

	for (b = bd; b; b = b->nxt)
	{	if (free_bound)
		{	n = free_bound;
			free_bound = n->nxt;
		} else
		{	n = (Bound *) emalloc(sizeof(Bound), 106);
		}
		n->bdef = b->bdef;
		n->ref  = b->ref;
		n->nxt  = m->bind;
		m->bind = n;
	}
	m->nxt = matches;
	matches = m;
	p_matched++;

	if (stream == 1)	// when streaming, print matches when found
	{	Prim *c, *r;
		if (json_format)
		{	fprintf(fd, "%s {\n", (nr_json>0)?",":"[");
			if (f && t)
			{	if (strcmp(f->fnm, t->fnm) == 0)
				{	sprintf(json_msg, "lines %d..%d",
						f->lnr, t->lnr);
				} else
				{	sprintf(json_msg, "lines %s:%d..%s:%d",
						f->fnm, f->lnr, t->fnm, t->lnr);
				}
			} else
			{	sprintf(json_msg, "lines %d:%d",
					f?f->lnr:0, t?t->lnr:0);
			}
			memset(bvars, 0, sizeof(bvars));
			for (b = bd; b; b = b->nxt)
			{	if (b->ref
				&&  strlen(b->ref->txt) + strlen(bvars) + 3 < sizeof(bvars))
				{	if (bvars[0] != '\0')
					{	strcat(bvars, ", ");	// add_match
					}
					strcat(bvars, b->ref->txt);
			}	}
			json_match(glob_te, json_msg, f, t); // add_match, streaming
			fprintf(fd, "}");
			nr_json++;
		} else
		{	fprintf(fd, "stdin:%d: ", f->lnr);
			for (c = r = f; c; c = c->nxt)
			{	fprintf(fd, "%s ", c->txt);
				if (c->lnr != r->lnr)
				{	fprintf(fd, "\nstdin:%d: ", c->lnr);
					r = c;
				}
				if (c == t)
				{	break;
			}	}
			fprintf(fd, "\n");
		}
		if (verbose && bd && bd->ref)
		{	fprintf(fd, "bound variables matched: ");
			while (bd && bd->ref)
			{	fprintf(fd, "%s ", bd->ref->txt);
				bd = bd->nxt;
			}
			fprintf(fd, "\n");
	}	}
}

void
new_named_set(const char *setname)
{	Named *q;

	for (q = namedset; q; q = q->nxt)
	{	if (strcmp(q->nm, setname) == 0)
		{	fprintf(stderr, "warning: set name %s already exists\n", q->nm);
			fprintf(stderr, "         this new copy hides the earlier one\n");
			break;
	}	}


	q = (Named *) emalloc(sizeof(Named), 100);
	q->nm = (char *) emalloc(strlen(setname)+1, 100);	// new_named_set
	strcpy(q->nm, setname);
	q->m = matches;
	q->nxt = namedset;
	namedset = q;
}

void
add_pattern(const char *s, const char *msg, Prim *from, Prim *upto)
{	Named *x;
	Match *om = matches;		// save old value

	matches = NULL;
	for (x = namedset; x; x = x->nxt)
	{	if (strcmp(x->nm, s) == 0)
		{	break;
	}	}
	if (!x)
	{	// strcpy(SetName, s);
		new_named_set((const char *) s);
		x = namedset;
	}
	add_match(from, upto, NULL);	// new pattern now only one in matches
	matches->nxt = x->m;		// if we're adding to the set
	if (msg)
	{	matches->msg = (char *) emalloc(strlen(msg)+1, 107);
		strcpy(matches->msg, msg);
	} else
	{	matches->msg = 0;
	}
	x->m = matches;			// there's only one
	x->cloned = NULL;
	matches = om;			// restore
	p_matched++;
}

void
del_pattern(const char *s, Prim *from, Prim *upto)
{	Named *x;
	Match *m, *om;

	for (x = namedset; x; x = x->nxt)
	{	if (strcmp(x->nm, s) == 0)
		{	break;
	}	}
	if (!x)
	{	if (verbose)
		{	fprintf(stderr, "warning: del_pattern: no such set '%s'\n", s);
		}
		return;
	}
	om = NULL;
	for (m = x->m; m; om = m, m = m->nxt)
	{	if (m->from == from
		&&  m->upto == upto)
		{	if (om)
			{	om->nxt = m->nxt;
			} else
			{	x->m = m->nxt;
			}	// could recycle m
			x->cloned = NULL;
			return;
	}	}
	if (verbose)
	{	fprintf(stderr, "warning: del_pattern: pattern not found in set %s\n", s);
	}
}

static void
cobra_export(const Prim *from, const Prim *upto)
{	int n = 0, m = 0;
	Prim *q = (Prim *) from;
	FILE *fd = (track_fd) ? track_fd : stdout;

	if (!q)
	{	return;
	}

	while (q && q->lnr == from->lnr)
	{	n++;
		q = q->prv;
	}
	if (upto && upto->seq > from->seq)
	{	q = (Prim *) from;
		while (q && q->seq < upto->seq)
		{	m++;
			q = q->nxt;
	}	}
	// last field of json record
	fprintf(fd, "    \"cobra\"\t:\t\"%d %d %d\"\n", no_cpp, n, m);
}

static int
json_add(const char *tp, const char *msg, const char *fnm, const int lnr, int pos, int len)
{	Files *f;
	Prim *p, *q;
	int cnt = 0;
	static char lastname[516];

	f = findfile(fnm);

	if (!f)
	{	if (strcmp(lastname, fnm) != 0)
		{	fprintf(stderr, "warning: cannot find '%s'\n", fnm);
		}
		strncpy(lastname, fnm, sizeof(lastname)-1);
		return 0;
	}
	p = f->first_token;
	while (p && p != f->last_token)
	{	if (p->lnr == lnr)
		{	while (p && pos > 0)
			{	p = p->nxt;
				pos--;
			}
			p->mark++;
			cnt++;
			q = p;
			while (q && len > 0)
			{	q = q->nxt;
				q->mark++;
				cnt++;
				len--;
			}
			if (tp && strlen(tp) > 0)
			{	add_pattern(tp, msg, p, q);
			}
			break; // marked token position(s)
		}
		p = p->nxt;
	}
	return cnt;
}

static int
json_add2(const char *tp, const char *msg, const char *fnm, const int lnr)	// if we have no pos and len
{	Files *f;
	Prim *p;
	int cnt = 0;
	static char lastname[516];

	f = findfile(fnm);
	if (!f)
	{	if (strcmp(lastname, fnm) != 0)
		{	fprintf(stderr, "warning: cannot find '%s'\n", fnm);
		}
		strncpy(lastname, fnm, sizeof(lastname)-1);
		return 0;
	}
	p = f->first_token;
	while (p && p != f->last_token)
	{	if (p->lnr == lnr)
		{	while (p && p->lnr == lnr)
			{	if (tp && strlen(tp) > 0)
				{	add_pattern(tp, msg, p, p);
				}
				p->mark++;
				p = p->nxt;
				cnt++;
			}
			break; // marked token position(s)
		}
		p = p->nxt;
	}
	return cnt;
}

void
json_import(const char *f)
{	FILE *fd;
	char *q, buf[1024];
	char fnm[512];
	char Type[512];
	char Msg[512];
	int a, b, c, lnr = -1;
	int once = 0;
	int cnt = 0;
	int ntp = 0;
	int nln = 0;
	int nfl = 0;
	int ncb = 0;
	int phase = 0;

	if ((fd = fopen(f, "r")) == NULL)
	{	fprintf(stderr, "cobra: cannot find file '%s'\n", f);
		return;
	}
	// we assume that every json record contains at least
	// the three fields: "type", "file", and "line"
	// and optionally also "cobra" which must be the last field
	// the rhs details for all except line must be enclosed in ""
again:
	strcpy(fnm, "");
	strcpy(Type, "");
	strcpy(Msg, "");
	while (fgets(buf, sizeof(buf), fd) != NULL)
	{	if ((q = strstr(buf, "\"type\"")) != NULL)
		{	q += strlen("\"type\"");
			while (*q != '\0' && *q != '"')
			{	q++;
			}
			if (*q == '"')
			{	char *r = q+1;
				while (*r != '\0' && *r != '"' && !isspace((int) *r))
				{	r++;
				}
				*r = '\0';
				strncpy(Type, q+1, sizeof(Type)-1);
				ntp++;
				if (phase == 1
				&&  strlen(fnm) > 0 && lnr >= 0)
				{	cnt += json_add2(Type, Msg, fnm, lnr);
					strcpy(Type, "");
					lnr = -1;
			}	}
			continue;
		}
		if ((q = strstr(buf, "\"line\"")) != NULL)
		{	q += strlen("\"line\"");
			while (*q != '\0' && !isdigit((int) *q))
			{	q++;
			}
			if (isdigit((int) *q))
			{	lnr = atoi(q);
				nln++;
				if (phase == 1
				&& strlen(Type) > 0 && strlen(fnm) > 0)
				{	cnt += json_add2(Type, Msg, fnm, lnr);
					strcpy(fnm, "");
					lnr = -1;
			}	}
			continue;
		}
		if ((q = strstr(buf, "\"file\"")) != NULL)
		{	q += strlen("\"file\"");
			while (*q != '\0' && *q != '"')
			{	q++;
			}
			if (*q == '"')
			{	char *r = q+1;
				while (*r != '\0' && *r != '"')
				{	r++;
				}
				*r = '\0';
				strncpy(fnm, q+1, sizeof(fnm)-1);
				nfl++;
				if (phase == 1
				&& strlen(Type) > 0 && lnr >= 0)
				{	cnt += json_add2(Type, Msg, fnm, lnr);
					strcpy(fnm, "");
					lnr = -1;
			}	}
			continue;
		}
		if ((q = strstr(buf, "\"message\"")) != NULL)
		{	q += strlen("\"message\"");
			while (*q != '\0' && *q != '"')
			{	q++;
			}
			if (*q == '"')
			{	char *r = q+1;
				while (*r != '\0' && *r != '"')
				{	r++;
				}
				*r = '\0';
				strncpy(Msg, q+1, sizeof(Msg)-1);
			}
			continue;
		}
		q = buf;
		if (phase == 0
		&&  (q = strstr(q, "\"cobra\"")) != NULL)
		{	q += strlen("\"cobra\"");
			q = strchr(q, '"');
			if (q
			&&  strlen(Type) > 0
			&&  strlen(fnm) > 0
			&&  lnr >= 0
			&&  sscanf(q, "\"%d %d %d\"", &a, &b, &c) == 3)
			{	cnt += json_add(Type, Msg, fnm, lnr, b, c);
				ncb++;
				if (no_cpp != a && !once++)
				{	fprintf(stderr, "warning: preprocessing is %sabled\n", no_cpp?"dis":"en");
					fprintf(stderr, "warning: but '%s' was written with%s preprocessing\n",
						f, a?"out":"");
					fprintf(stderr, "warning: which can affect location accuray\n");
				}
				strcpy(fnm, "");
				lnr = -1;
	}	}	}
	if (phase == 1
	&&  strlen(Type) > 0
	&&  strlen(fnm) > 0
	&&  lnr >= 0)
	{	json_add2(Type, Msg, fnm, lnr);	// final match
	}
	fclose(fd);

	if (phase == 0 && ncb == 0)
	{	phase = 1;
		if (ntp == 0 || nfl == 0 || nln == 0)
		{	fprintf(stderr, "warning: no useable records found (file, line, or cobra fields missing)\n");
		} else
		{	if (nfl != nln || ntp != nfl)
			{	fprintf(stderr, "warning: not all records have type, file and line fields\n");
			}
			if ((fd = fopen(f, "r")) != NULL)	// reopen
			{	goto again;
	}	}	}

	if (!no_display && !no_match)
	{	printf("imported %d records, marked %d tokens\n", ntp, cnt);
	}
}

void
json_match(const char *te, const char *msg, const Prim *from, const Prim *upto)
{	const char *f = from?from->fnm:"";
	const int ln  = from?from->lnr:0;
	FILE *fd = (track_fd) ? track_fd : stdout;

	// fields can only contain: alphanumeric _ . $ @
	// unless preceded by an escape \ character
	// cleaned_up inserts escapes where needed
	// unless in double quotes....

	if (!te
	||  !msg)
	{	return;
	}
	fprintf(fd, "  { \"type\"\t:\t\"");
	 cleaned_up(fd, te);
	fprintf(fd, "\",\n");

	fprintf(fd, "    \"message\"\t:\t\"");
	 cleaned_up(fd, msg);
	fprintf(fd, "\",\n");

	if (json_plus)
	{	if (strlen(bvars) > 0)		// json_match
		{	fprintf(fd, "    \"bindings\"\t:\t\"");
			cleaned_up(fd, bvars);
			fprintf(fd, "\",\n");
			strcpy(bvars, "");
		}
		fprintf(fd, "    \"source\"\t:\t\"");
		show_line(fd, f, 0, ln, ln, -1);
		fprintf(fd, "\",\n");
	}

	fprintf(fd, "    \"file\"\t:\t\"%s\",\n", f);
	fprintf(fd, "    \"line\"\t:\t%d,\n", ln);

	cobra_export(from, upto); // 3 numbers
	fprintf(fd, "  }\n");
}

int
do_markups(const char *setname)
{	Match *m;
	Bound *b;
	int seq = 1;
	int w = 0;
	int p = 0;

	for (m = matches; m; seq++, m = m->nxt)
	{	p++;	// nr of patterns
		if (m->bind)
		{	if (verbose && w++ == 0)
			{	printf("bound variables matched:\n");
			}
			for (b = m->bind; b; b = b->nxt)
			{
			//	if (!b->ref)
			//	{	continue;
			//	}
				w++;	// nr of bound vars
				if (strlen(setname) == 0)	// needed?
				{	if (b->ref)
					{	b->ref->mark |= 4;	// match of bound variable
					} else	// eg ref of bound var in constraint
					{	// m->from->mark |= 4;	// mark the start of the pattern
				}	}
				if (b->bdef)
				{	b->bdef->mark |= 16;	// location bound variable defined
				}
				if (verbose)
				{	printf("\t%d:%d %s:%d: %s (line %d)\n", seq, w,
						b->ref?b->ref->fnm:"",
						b->ref?b->ref->lnr:0,
						b->ref?b->ref->txt:"",
						b->bdef?b->bdef->lnr:0);
	}	}	}	}

	if (!json_format && !no_match && !no_display)
	{	if (strlen(setname) == 0)
		{	printf("%d patterns\n", p);
		}
		if (w > 0)
		{	printf("%d bound variable%s\n",
				w, (w==1)?"":"s");
	}	}

	return p;
}

int
matches2marks(int doclear)
{	Match	*m;
	Prim	*p;
	int	loc_cnt;

	loc_cnt = 0;
	for (m = matches; m; m = m->nxt)
	{	p = m->from;
		p->bound = m->upto;
		if (p)
		{	if (!p->mark)
			{	loc_cnt++;
			}
			p->mark |= 2;	// start of pattern
			for (; p; p = p->nxt)
			{	if (!p->mark)
				{	loc_cnt++;
				}
				p->mark |= 1;
				if (p == m->upto)
				{	p->mark |= 8;
					break;
	}	}	}	}

	if (doclear)
	{	clear_matches();
	}
	if (!json_format && !no_match && !no_display)
	{	printf("%d token%s marked\n", loc_cnt, (loc_cnt==1)?"":"s");
	}

	return loc_cnt;
}

int
json_convert(const char *s)	// convert pattern matches to marks
{	Match *m = matches;
	int n = 0;

	matches = findset(s, 1, 1);
	if (matches)
	{	(void) do_markups(s);
		n = matches2marks(0);
	}
	matches = m;
	return n;
}

void
json(const char *te)
{	Prim *sop = (Prim *) 0;
	Prim *q, *mycur;
	int seen = 0;
	FILE *fd = (track_fd) ? track_fd : stdout;

	// usage:
	//	json		# prints json format summary of all matches
	//	json string	# puts string the type field

	// tokens that are part of a match are marked         &1
	// the token at start of each pattern match is marked &2
	// tokens that match a bound variable are marked      &4
	// the token at end of each pattern match is marked   &8
	// the token where a bound variable is defined        &16

	// if the json argument is a number or a single *, then
	// the output is verbose for that match (or all, for a *)

	memset(bvars, 0, sizeof(bvars));
	fprintf(fd, "[\n");
	for (mycur = prim; mycur; mycur = mycur?mycur->nxt:0)
	{	if (mycur->mark)
		{	seen |= 2;
		}
		if (!(mycur->mark & 2))	// find start of match
		{	continue;
		}
		if (sop)
		{	fprintf(fd, ",\n");
		}
		// start of match
		sop = mycur;

		q = mycur;
		if (q->bound)	// assume this wasnt set for other reasons...
		{	if (strcmp(q->fnm, q->bound->fnm) == 0)
			{	sprintf(json_msg, "lines %d..%d", q->lnr, q->bound->lnr);
			} else
			{	sprintf(json_msg, "lines %s:%d..%s:%d",
					q->fnm, q->lnr, q->bound->fnm, q->bound->lnr);
			}
			while (q && q->mark > 0)
			{	check_bvar(q);
				q = q->nxt;
			}
		} else
		{	while (q && q->mark > 0)
			{	check_bvar(q);
				if (q->nxt)
				{	q = q->nxt;
				} else
				{	break;
			}	}
			if (strcmp(sop->fnm, q?q->fnm:"") == 0)
			{	sprintf(json_msg, "lines %d..%d", sop->lnr, q?q->lnr:0);
			} else
			{	sprintf(json_msg, "lines %s:%d..%s:%d",
					sop->fnm, sop->lnr, q?q->fnm:"", q?q->lnr:0);
		}	}
		json_match(te, json_msg, sop, q);	// json()
		seen |= 4;
	}
	if (seen == 2)	// saw marked tokens, but no patterns, find ranges to report
	{	for (mycur = prim; mycur; mycur = mycur?mycur->nxt:0)
		{	if (!mycur->mark)
			{	continue;
			}
			sop = mycur;
			while (mycur && mycur->mark)
			{	if (mycur->nxt)
				{	mycur = mycur->nxt;
				} else
				{	break;
			}	}
			sprintf(json_msg, "lines %d..%d", sop->lnr, mycur?mycur->lnr:0);
			json_match(te, json_msg, sop, mycur); // json()
		}
	}
	fprintf(fd, "\n]\n");
}

void
show_line(FILE *fdo, const char *fnm, int n, int from, int upto, int tag)
{	FILE *fdi;
	char buf[MAXYYTEXT];
	int ln = 0;
	static int wcnt = 1;

	if (scrub && strlen(scrub_caption) > 0)
	{	fprintf(fdo, "cobra%05d <Med> :%s:%d: %s\n",
			wcnt++, fnm, tag, scrub_caption);
		if (upto == from)
		{	return;
	}	}

	if ((fdi = fopen(fnm, "r")) == NULL)
	{	fprintf(stderr, "cannot open '%s'\n", fnm);
		return;
	}

	while (fgets(buf, sizeof(buf), fdi) && ++ln <= upto)
	{	if (ln >= from)
		{	if (scrub)
			{	if (buf[0] == '\n' || buf[0] == '\r')
				{	continue;
				}
				if (strlen(scrub_caption) > 0)
				{	fprintf(fdo, "\t");
				} else
				{	fprintf(fdo, "cobra%05d <Med> :%s:%d: ",
						wcnt++, fnm, ln);
				}
				fprintf(fdo, "%c", (ln == tag && upto > from)?'>':' ');
				goto L;
			}
			if (n > 0 && !cobra_texpr)
			{	fprintf(fdo, "%3d: ", n);
			}
			if (gui)
			{	fprintf(fdo, "%c%s:%05d:  ",
					(ln == tag && upto > from)?'>':' ',
					fnm, ln);
			} else if (tag >= 0)
			{	fprintf(fdo, "%c%5d  ",
					(ln == tag && upto > from)?'>':' ',
					ln);
			} else	// tag < 0 -> json output
			{	char *q, *p = buf;
				while (*p == ' ' || *p == '\t')
				{	p++;
				}
				while ((q = strchr(p, '\n'))
				||     (q = strchr(p, '\r')))
				{	*q = '\0';
				}
#if 1
				if ((q = strstr(p, "//")) != NULL)
				{	*q = '\0'; // strip comments
				}
				cleaned_up(fdo, p);
#else
				// remove escape characters and double quotes
				for (q = p; *q != '\0'; q++)
				{	if (*q == '"')
					{	*q = '\'';
					} else if (*q == '\\')
					{	*q = ' ';
					} else if (*q == '/' && *(q+1) == '/')
					{	*q = '\0';
						break;
				}	}
				fprintf(fdo, "%s", p);
#endif
				continue;
			}
L:			fprintf(fdo, "%s", buf);
	}	}

	fclose(fdi);
}
