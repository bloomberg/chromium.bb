#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

from util import build_utils
from util import proguard_util


_DANGEROUS_OPTIMIZATIONS = [
    # See crbug.com/825995 (can cause VerifyErrors)
    "class/merging/vertical",
    "class/unboxing/enum",
    # See crbug.com/625992
    "code/allocation/variable",
    # See crbug.com/625994
    "field/propagation/value",
    "method/propagation/parameter",
    "method/propagation/returnvalue",
]

def _ParseOptions(args):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--proguard-path',
                    help='Path to the proguard executable.')
  parser.add_option('--input-paths',
                    help='Paths to the .jar files proguard should run on.')
  parser.add_option('--output-path', help='Path to the generated .jar file.')
  parser.add_option('--proguard-configs', action='append',
                    help='Paths to proguard configuration files.')
  parser.add_option('--proguard-config-exclusions',
                    default='',
                    help='GN list of paths to proguard configuration files '
                         'included by --proguard-configs, but that should '
                         'not actually be included.')
  parser.add_option('--mapping', help='Path to proguard mapping to apply.')
  parser.add_option('--is-test', action='store_true',
      help='If true, extra proguard options for instrumentation tests will be '
      'added.')
  parser.add_option('--classpath', action='append',
                    help='Classpath for proguard.')
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option('--enable-dangerous-optimizations', action='store_true',
                    help='Enable optimizations which are known to have issues.')
  parser.add_option('--verbose', '-v', action='store_true',
                    help='Print all proguard output')

  options, _ = parser.parse_args(args)

  classpath = []
  for arg in options.classpath:
    classpath += build_utils.ParseGnList(arg)
  options.classpath = classpath

  configs = []
  for arg in options.proguard_configs:
    configs += build_utils.ParseGnList(arg)
  options.proguard_configs = configs
  options.proguard_config_exclusions = (
      build_utils.ParseGnList(options.proguard_config_exclusions))

  options.input_paths = build_utils.ParseGnList(options.input_paths)

  return options


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseOptions(args)

  proguard = proguard_util.ProguardCmdBuilder(options.proguard_path)
  proguard.injars(options.input_paths)
  proguard.configs(options.proguard_configs)
  proguard.config_exclusions(options.proguard_config_exclusions)
  proguard.outjar(options.output_path)

  if options.mapping:
    proguard.mapping(options.mapping)

  classpath = list(set(options.classpath))
  proguard.libraryjars(classpath)
  proguard.verbose(options.verbose)
  if not options.enable_dangerous_optimizations:
    proguard.disable_optimizations(_DANGEROUS_OPTIMIZATIONS)

  build_utils.CallAndWriteDepfileIfStale(
      proguard.CheckOutput,
      options,
      input_paths=proguard.GetInputs(),
      input_strings=proguard.build(),
      output_paths=proguard.GetOutputs(),
      depfile_deps=proguard.GetDepfileDeps())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
