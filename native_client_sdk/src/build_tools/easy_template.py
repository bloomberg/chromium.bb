#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import cStringIO
import optparse
import re
import sys


STATEMENT_RE = "\[\[(.*?)\]\]"  # [[...]]
EXPR_RE = "\{\{(.*?)\}\}"  # {{...}}

def TemplateToPython(template, statement_re, expr_re):
  output = cStringIO.StringIO()
  indent_re = re.compile(r'\s*')
  indent_string = ''
  for line in template.splitlines(1):  # 1 => keep line ends
    m = statement_re.match(line)
    if m:
      statement = m.group(1)
      indent_string = indent_re.match(statement).group()
      if statement.rstrip()[-1:] == ':':
        indent_string += '  '
      output.write(statement + '\n')
    else:
      line_ending = ''
      while line and line[-1] in '\\"\n\r':
        line_ending = line[-1] + line_ending
        line = line[:-1]

      m = expr_re.search(line)
      if m:
        line = line.replace('%', '%%')
        subst_line = r'r"""%s""" %% (%s,)' % (
            re.sub(expr_re, '%s', line),
            ', '.join(re.findall(expr_re, line)))
      else:
        subst_line = r'r"""%s"""' % line

      out_string = r'%s__outfile__.write(%s + %s)' % (
          indent_string,
          subst_line,
          repr(line_ending))
      output.write(out_string + '\n')

  return output.getvalue()


def RunTemplate(src, dst, template_dict, statement_re=None, expr_re=None):
  statement_re = statement_re or re.compile(STATEMENT_RE)
  expr_re = expr_re or re.compile(EXPR_RE)
  script = TemplateToPython(src.read(), statement_re, expr_re)
  template_dict = copy.copy(template_dict)
  template_dict['__outfile__'] = dst
  exec script in template_dict


def RunTemplateFile(srcfile, dstfile, template_dict, statement_re=None,
                    expr_re=None):
  with open(srcfile) as src:
    with open(dstfile, 'w') as dst:
      RunTemplate(src, dst, template_dict, statement_re, expr_re)


def main(args):
  parser = optparse.OptionParser()
  _, args = parser.parse_args(args)
  if not args:
    return

  with open(args[0]) as f:
    print TemplateToPython(
        f.read(), re.compile(STATEMENT_RE), re.compile(EXPR_RE))

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
