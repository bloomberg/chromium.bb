#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import re
import sys


verbose = False


def quote(input_str, specials, escape='\\'):
  """Returns a quoted version of |str|, where every character in the
  iterable |specials| (usually a set or a string) and the escape
  character |escape| is replaced by the original character preceded by
  the escape character."""

  assert len(escape) == 1

  # Since escape is used in replacement pattern context, so we need to
  # ensure that it stays a simple literal and cannot affect the \1
  # that will follow it.
  if escape == '\\':
    escape = '\\\\'
  if len(specials) > 0:
    return re.sub(r'(' + r'|'.join(specials)+r'|'+escape + r')',
                  escape + r'\1', input_str)
  return re.sub(r'(' + escape + r')', escape + r'\1', input_str)


def unquote(input_str, specials, escape='\\'):
  """Splits the input string |input_str| where special characters in
  the input |specials| are, if not quoted by |escape|, used as
  delimiters to split the string.  The returned value is a list of
  strings of alternating non-specials and specials used to break the
  string. The list will always begin with a possibly empty string of
  non-specials, but may end with either specials or non-specials."""

  assert len(escape) == 1

  out = []
  cur_out = []
  cur_special = False
  lit_next = False
  for c in input_str:
    if cur_special:
      lit_next = (c == escape)
      if c not in specials or lit_next:
        cur_special = False
        out.append(''.join(cur_out))
        if not lit_next:
          cur_out = [c]
        else:
          cur_out = []
      else:
        cur_out.append(c)
    else:
      if lit_next:
        cur_out.append(c)
        lit_next = False
      else:
        lit_next = c == escape
        if c not in specials:
          if not lit_next:
            cur_out.append(c)
        else:
          out.append(''.join(cur_out))
          cur_out = [c]
          cur_special = True
  out.append(''.join(cur_out))
  return out


# Test code

def VerboseQuote(in_string, specials, *args, **kwargs):
  if verbose:
    sys.stdout.write('Invoking quote(%s, %s, %s)\n' %
                     (repr(in_string), repr(specials),
                      ', '.join([repr(a) for a in args] +
                                [repr(k) + ':' + repr(v)
                                 for k, v in kwargs])))
  return quote(in_string, specials, *args, **kwargs)


def VerboseUnquote(in_string, specials, *args, **kwargs):
  if verbose:
    sys.stdout.write('Invoking unquote(%s, %s, %s)\n' %
                     (repr(in_string), repr(specials),
                      ', '.join([repr(a) for a in args] +
                                [repr(k) + ':' + repr(v)
                                 for k, v in kwargs])))
  return unquote(in_string, specials, *args, **kwargs)


def TestGeneric(fn, in_args, expected_out_obj):
  sys.stdout.write('[ RUN      ] %s(%s, %s)\n' %
                   (fn.func_name, repr(in_args), repr(expected_out_obj)))
  actual = apply(fn, in_args)
  if actual != expected_out_obj:
    sys.stdout.write('[    ERROR ] expected %s, got %s\n' %
                     (str(expected_out_obj), str(actual)))
    return 1
  sys.stdout.write('[       OK ]\n')
  return 0


def TestInvertible(in_string, specials, escape='\\'):
  sys.stdout.write('[ RUN      ] TestInvertible(%s, %s, %s)\n' %
                   (repr(in_string), repr(specials), repr(escape)))
  q = VerboseQuote(in_string, specials, escape)
  qq = VerboseUnquote(q, specials, escape)
  if in_string != ''.join(qq):
    sys.stdout.write(('[    ERROR ] TestInvertible(%s, %s, %s) failed, '
                      'expected %s, got %s\n') %
                     (repr(in_string), repr(specials), repr(escape),
                      repr(in_string), repr(''.join(qq))))
    return 1
  return 0


def RunAllTests():
  test_tuples = [[VerboseQuote,
                  ['foo, bar, baz, and quux too!', 'abc'],
                  'foo, \\b\\ar, \\b\\az, \\and quux too!'],
                 [VerboseQuote,
                  ['when \\ appears in the input', 'a'],
                  'when \\\\ \\appe\\ars in the input'],
                 [VerboseUnquote,
                  ['key\\:still_key:value\\:more_value', ':'],
                  ['key:still_key', ':', 'value:more_value']],
                 [VerboseUnquote,
                  ['about that sep\\ar\\ator in the beginning', 'ab'],
                  ['', 'ab', 'out th', 'a', 't separator in the ',
                   'b', 'eginning']],
                 [VerboseUnquote,
                  ['the rain in spain fall\\s ma\\i\\nly on the plains', 'ins'],
                  ['the ra', 'in', ' ', 'in', ' ', 's', 'pa', 'in',
                   ' falls mainly o', 'n', ' the pla', 'ins']],
                 ]
  num_errors = 0
  for func, in_args, expected in test_tuples:
    num_errors += TestGeneric(func, in_args, expected)
  num_errors += TestInvertible('abcdefg', 'bc')
  num_errors += TestInvertible('a\\bcdefg', 'bc')
  num_errors += TestInvertible('ab\\cdefg', 'bc')
  num_errors += TestInvertible('\\ab\\cdefg', 'abc')
  num_errors += TestInvertible('abcde\\fg', 'efg')
  num_errors += TestInvertible('a\\b', '')
  return num_errors


# Invoke this file directly for simple testing.

def main(argv):
  global verbose
  parser = optparse.OptionParser(
    usage='Usage: %prog [options] word...')
  parser.add_option('-s', '--special-chars', dest='special_chars', default=':',
                    help='Special characters to quote (default is ":")')
  parser.add_option('-q', '--quote', dest='quote', default='\\',
                    help='Quote or escape character (default is "\")')
  parser.add_option('-t', '--run-tests', dest='tests', action='store_true',
                    help='Run built-in tests\n')
  parser.add_option('-v', '--verbose', dest='verbose', action='store_true',
                    help='Verbose test output')
  options, args = parser.parse_args(argv)

  if options.verbose:
    verbose = True

  num_errors = 0
  if options.tests:
    num_errors = RunAllTests()
  for word in args:
    q = quote(word, options.special_chars, options.quote)
    qq = unquote(q, options.special_chars, options.quote)
    sys.stdout.write('quote(%s) = %s, unquote(%s) = %s\n'
                     % (word, q, q, ''.join(qq)))
    if word != ''.join(qq):
      num_errors += 1
  if num_errors > 0:
    sys.stderr.write('[  FAILED  ] %d test failures\n' % num_errors)
  return num_errors

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

