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
  classpath = build_utils.ParseGypList(options.classpath)
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
  build_utils.CheckOutput(proguard_cmd, print_stdout=True)


def main():
  parser = optparse.OptionParser()
  parser.add_option('--proguard-path',
                    help='Path to the proguard executable.')
  parser.add_option('--input-path',
                    help='Path to the .jar file proguard should run on.')
  parser.add_option('--output-path', help='Path to the generated .jar file.')
  parser.add_option('--proguard-config',
                    help='Path to the proguard configuration file.')
  parser.add_option('--classpath', help="Classpath for proguard.")
  parser.add_option('--stamp', help='Path to touch on success.')

  options, _ = parser.parse_args()

  DoProguard(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main())
