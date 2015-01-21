#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Renders one or more template files using the Jinja template engine."""

import codecs
import optparse
import os
import sys

from util import build_utils

# Import jinja2 from third_party/jinja2
sys.path.append(os.path.join(os.path.dirname(__file__), '../../../third_party'))
import jinja2  # pylint: disable=F0401


def ProcessFile(input_filename, output_filename, variables):
  with codecs.open(input_filename, 'r', 'utf-8') as input_file:
    input_ = input_file.read()
  env = jinja2.Environment(undefined=jinja2.StrictUndefined)
  template = env.from_string(input_)
  template.filename = os.path.abspath(input_filename)
  output = template.render(variables)
  with codecs.open(output_filename, 'w', 'utf-8') as output_file:
    output_file.write(output)


def ProcessFiles(input_filenames, inputs_base_dir, outputs_zip, variables):
  with build_utils.TempDir() as temp_dir:
    for input_filename in input_filenames:
      relpath = os.path.relpath(os.path.abspath(input_filename),
                                os.path.abspath(inputs_base_dir))
      if relpath.startswith(os.pardir):
        raise Exception('input file %s is not contained in inputs base dir %s'
                        % input_filename, inputs_base_dir)

      output_filename = os.path.join(temp_dir, relpath)
      parent_dir = os.path.dirname(output_filename)
      build_utils.MakeDirectory(parent_dir)
      ProcessFile(input_filename, output_filename, variables)

    build_utils.ZipDir(outputs_zip, temp_dir)


def main():
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--inputs', help='The template files to process.')
  parser.add_option('--output', help='The output file to generate. Valid '
                    'only if there is a single input.')
  parser.add_option('--outputs-zip', help='A zip file containing the processed '
                    'templates. Required if there are multiple inputs.')
  parser.add_option('--inputs-base-dir', help='A common ancestor directory of '
                    'the inputs. Each output\'s path in the output zip will '
                    'match the relative path from INPUTS_BASE_DIR to the '
                    'input. Required if --output-zip is given.')
  parser.add_option('--variables', help='Variables to be made available in the '
                    'template processing environment, as a GYP list (e.g. '
                    '--variables "channel=beta mstone=39")', default='')
  options, args = parser.parse_args()

  build_utils.CheckOptions(options, parser, required=['inputs'])
  inputs = build_utils.ParseGypList(options.inputs)

  if (options.output is None) == (options.outputs_zip is None):
    parser.error('Exactly one of --output and --output-zip must be given')
  if options.output and len(inputs) != 1:
    parser.error('--output cannot be used with multiple inputs')
  if options.outputs_zip and not options.inputs_base_dir:
    parser.error('--inputs-base-dir must be given when --output-zip is used')
  if args:
    parser.error('No positional arguments should be given.')

  variables = {}
  for v in build_utils.ParseGypList(options.variables):
    if '=' not in v:
      parser.error('--variables argument must contain "=": ' + v)
    name, _, value = v.partition('=')
    variables[name] = value

  if options.output:
    ProcessFile(inputs[0], options.output, variables)
  else:
    ProcessFiles(inputs, options.inputs_base_dir, options.outputs_zip,
                 variables)

  if options.depfile:
    deps = inputs + build_utils.GetPythonDependencies()
    build_utils.WriteDepfile(options.depfile, deps)


if __name__ == '__main__':
  main()
