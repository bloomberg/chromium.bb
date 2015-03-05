#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

from util import build_utils

def DoProguard(options):
  injars = options.input_path
  outjars = options.output_path
  classpath = []
  for arg in options.classpath:
    classpath += build_utils.ParseGypList(arg)
  classpath = list(set(classpath))
  libraryjars = ':'.join(classpath)
  # proguard does its own dependency checking, which can be avoided by deleting
  # the output.
  if os.path.exists(options.output_path):
    os.remove(options.output_path)
  proguard_cmd = ['java', '-jar',
                  options.proguard_path,
                  '-injars', injars,
                  '-outjars', outjars,
                  '-libraryjars', libraryjars,
                  '@' + options.proguard_config]
  build_utils.CheckOutput(proguard_cmd, print_stdout=True,
                          stdout_filter=FilterProguardOutput)


def FilterProguardOutput(output):
  '''ProGuard outputs boring stuff to stdout (proguard version, jar path, etc)
  as well as interesting stuff (notes, warnings, etc). If stdout is entirely
  boring, this method suppresses the output.
  '''
  ignore_patterns = [
    'ProGuard, version ',
    'Reading program jar [',
    'Reading library jar [',
    'Preparing output jar [',
    '  Copying resources from program jar [',
  ]
  for line in output.splitlines():
    for pattern in ignore_patterns:
      if line.startswith(pattern):
        break
    else:
      # line doesn't match any of the patterns; it's probably something worth
      # printing out.
      return output
  return ''


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--proguard-path',
                    help='Path to the proguard executable.')
  parser.add_option('--input-path',
                    help='Path to the .jar file proguard should run on.')
  parser.add_option('--output-path', help='Path to the generated .jar file.')
  parser.add_option('--proguard-config',
                    help='Path to the proguard configuration file.')
  parser.add_option('--classpath', action='append',
                    help="Classpath for proguard.")
  parser.add_option('--stamp', help='Path to touch on success.')

  options, _ = parser.parse_args(args)

  DoProguard(options)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        build_utils.GetPythonDependencies())

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
