#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import optparse
import os
import sys

import buildbot_common
import build_projects
import build_sdk
import build_version
import parse_dsc

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_LIBRARY_DIR = os.path.join(SDK_SRC_DIR, 'libraries')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
OUT_DIR = os.path.join(SRC_DIR, 'out')

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos


TEST_EXAMPLE_LIST = [
  'nacl_io_test',
]

TEST_LIBRARY_LIST = [
  'gmock',
  'gtest',
  'gtest_ppapi',
]


def BuildStepBuildExamples(pepperdir, platform):
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'examples',
                             'Build Examples (Debug)',
                             deps=False, config='Debug')
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'examples',
                             'Build Examples (Release)',
                             deps=False, config='Release')


def BuildStepCopyTests(pepperdir, toolchains, build_experimental, clobber):
  buildbot_common.BuildStep('Copy Tests')

  # Update test libraries and test apps
  filters = {
    'DEST': ['testlibs', 'tests']
  }
  if not build_experimental:
    filters['EXPERIMENTAL'] = False

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  platform = getos.GetPlatform()
  build_projects.UpdateHelpers(pepperdir, platform, clobber=clobber)
  build_projects.UpdateProjects(pepperdir, platform, tree, clobber=clobber,
                                toolchains=toolchains)



def BuildStepBuildTests(pepperdir, platform):
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'testlibs',
                             'Build Test Libraries')
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'tests', 'Build Tests',
                             deps=False)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--experimental', help='build experimental tests',
                    action='store_true')

  options, args = parser.parse_args(args[1:])

  platform = getos.GetPlatform()
  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  toolchains = ['newlib', 'glibc', 'pnacl', 'host']

  BuildStepBuildExamples(pepperdir, platform)
  BuildStepCopyTests(pepperdir, toolchains, options.experimental, True)
  BuildStepBuildTests(pepperdir, platform)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
