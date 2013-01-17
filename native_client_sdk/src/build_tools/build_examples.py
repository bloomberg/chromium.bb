#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

import build_sdk
import build_utils
import test_sdk

sys.path.append(os.path.join(build_sdk.SDK_SRC_DIR, 'tools'))
import getos


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--copy',
      help='Only copy the files, don\'t build.',
      action='store_true' )
  parser.add_option('--clobber-examples',
      help='Don\'t examples directory before copying new files',
      action='store_true' )
  parser.add_option('--test-examples',
      help='Run the pyauto tests for examples.', action='store_true')
  parser.add_option('--experimental',
      help='build experimental examples and libraries', action='store_true')
  parser.add_option('-t', '--toolchain',
      help='Build using toolchain. Can be passed more than once.',
      action='append')
  parser.add_option('--gyp',
      help='Use gyp to build examples/libraries/Makefiles.',
      action='store_true')

  options, args = parser.parse_args(args[1:])

  valid_toolchains = ['newlib', 'glibc', 'pnacl', 'host']
  if not options.toolchain:
    toolchains = valid_toolchains
  else:
    invalid_toolchains = set(options.toolchain) - set(valid_toolchains)
    if invalid_toolchains:
      print 'Ignoring invalid toolchains: %s' % (', '.join(invalid_toolchains),)
    toolchains = list(set(options.toolchain) - invalid_toolchains)

  pepper_ver = str(int(build_utils.ChromeMajorVersion()))
  pepperdir = os.path.join(build_sdk.OUT_DIR, 'pepper_' + pepper_ver)
  platform = getos.GetPlatform()

  build_sdk.options = options

  build_sdk.BuildStepCopyBuildHelpers(pepperdir, platform)
  build_sdk.BuildStepCopyExamples(pepperdir, toolchains, options.experimental,
                                  options.clobber_examples)
  test_sdk.BuildStepCopyTests(pepperdir, toolchains, options.experimental,
                              options.clobber_examples)
  if options.copy:
    return 0

  # False = don't clean after building the libraries directory.
  build_sdk.BuildStepBuildLibraries(pepperdir, platform, 'src', False)
  test_sdk.BuildStepBuildExamples(pepperdir, platform)
  test_sdk.BuildStepBuildTests(pepperdir, platform)
  if options.test_examples:
    test_sdk.BuildStepRunPyautoTests(pepperdir, platform, pepper_ver)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
