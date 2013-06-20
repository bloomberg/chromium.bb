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


def BuildStepBuildExamples(pepperdir, platform):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, platform, 'examples',
                               'Build Examples (%s)' % config,
                               deps=False, config=config)


def BuildStepCopyTests(pepperdir, toolchains, build_experimental):
  buildbot_common.BuildStep('Copy Tests')

  # Update test libraries and test apps
  filters = {
    'DEST': ['testlibs', 'tests']
  }
  if not build_experimental:
    filters['EXPERIMENTAL'] = False

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  platform = getos.GetPlatform()
  build_projects.UpdateHelpers(pepperdir, platform, clobber=False)
  build_projects.UpdateProjects(pepperdir, platform, tree, clobber=False,
                                toolchains=toolchains)


def BuildStepBuildTests(pepperdir, platform):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, platform, 'testlibs',
                               'Build Test Libraries (%s)' % config,
                               config=config)
    build_sdk.BuildStepMakeAll(pepperdir, platform, 'tests',
                               'Build Tests (%s)' % config,
                               deps=False, config=config)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--experimental', help='build experimental tests',
                    action='store_true')

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # of the build.
    del os.environ['NACL_SDK_ROOT']

  options, args = parser.parse_args(args[1:])

  platform = getos.GetPlatform()
  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  toolchains = ['newlib', 'glibc', 'pnacl']
  toolchains.append(platform)

  BuildStepBuildExamples(pepperdir, platform)
  BuildStepCopyTests(pepperdir, toolchains, options.experimental)
  BuildStepBuildTests(pepperdir, platform)

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('test_sdk: interrupted')
