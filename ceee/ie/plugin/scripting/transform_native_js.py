# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''A GYP script action file to transform native JS function declarations.

Any line in the input script of the form
  "native function <NAME>(<ARGS>);"
will be transformed to
  "var <NAME> = ceee.<NAME>; // function(<ARGS>)"
'''

import optparse
import os.path
import re
import sys

_NATIVE_FUNCTION_RE = re.compile(
   'native\s+function\s+(?P<name>\w+)\((?P<args>[^)]*)\)\s*;')


def TransformFile(input_file, output_file):
  '''Transform native functions in input_file, write to output_file'''
  # Slurp the input file.
  contents = open(input_file, "r").read()

  repl = 'var \g<name> = ceee.\g<name>;  // function(\g<args>)'
  contents = _NATIVE_FUNCTION_RE.sub(repl, contents,)

  # Write the output file.
  open(output_file, "w").write(contents)


def GetOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.add_option('-o', dest='output_dir',
                    help='Output directory')
  return parser

def Main():
  parser = GetOptionParser()
  (opts, args) = parser.parse_args()
  if not opts.output_dir:
    parser.error('You must provide an output directory')

  for input_file in args:
    output_file = os.path.join(opts.output_dir, os.path.basename(input_file))
    TransformFile(input_file, output_file)

  return 0


if __name__ == '__main__':
  sys.exit(Main())
