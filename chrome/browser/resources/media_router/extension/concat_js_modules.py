# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script concatenates JS files into a set of output files.

Modules are specified as a list of strings of the form
module=<name>:<num>[:...], where <name> is the name of the module and
<num> is the number of JS files that should be concatenates to make
the module file, and if a second : is present, everything after it is
ignored.  (This strange format is used because it is compatible with
JSCompiler's --module flag.)
"""

import argparse
import os.path

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "--module-specs", nargs='+',
      help="List of module specifiations.")
  parser.add_argument(
      "--output-dir",
      help="Directory where output files are written.")
  parser.add_argument(
      "--prelude-file",
      help="A prelude file included in every output module.")
  parser.add_argument(
      "sources", nargs="+",
      help="JS input files.")
  args = parser.parse_args()

  # Read the prelude file, which contains code to be placed at the
  # start of each module.
  with open(args.prelude_file, 'r') as prelude_in:
    prelude = prelude_in.read()

  # Loop over all specified modules, and simulaneously traverse the
  # list of source files using `source_index`.
  source_index = 0
  for spec in args.module_specs:
    # Split the module spec into a module name and a count of source
    # files to include in the module.
    pre, sep, post = spec.partition('module=')
    assert pre == ''
    assert sep
    parts = post.split(':')
    module_name = parts[0]
    input_count = int(parts[1])

    # Write the module file.
    module_file = os.path.join(args.output_dir, module_name + ".js")
    with open(module_file, "w") as module_out:
      module_out.write(prelude)

      # Append as many input files as requested by the module spec.
      for i in range(input_count):
        source_file = args.sources[source_index]
        source_index += 1
        with open(source_file, "r") as source_in:
          module_out.write(source_in.read())

  # Check that every source file has been appended to a module.
  if source_index != len(args.sources):
    sys.exit("Too many input files.")

main()
