#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import copy
import optparse
import os
import sys

import buildbot_common
import build_utils
import build_sdk
import generate_make

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_LIBRARY_DIR = os.path.join(SDK_SRC_DIR, 'libraries')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
OUT_DIR = os.path.join(SRC_DIR, 'out')

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos


TEST_EXAMPLE_LIST = [
  'nacl_mounts_test',
]

TEST_LIBRARY_LIST = [
  'gmock',
  'gtest',
  'gtest_ppapi',
]


def BuildStepBuildExamples(pepperdir, platform):
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'examples', 'Build Examples')


def BuildStepCopyTests(pepperdir, toolchains, build_experimental, clobber):
  buildbot_common.BuildStep('Copy Tests')

  build_sdk.MakeDirectoryOrClobber(pepperdir, 'testlibs', clobber)
  build_sdk.MakeDirectoryOrClobber(pepperdir, 'tests', clobber)

  args = ['--dstroot=%s' % pepperdir, '--master']
  for toolchain in toolchains:
    args.append('--' + toolchain)

  for library in TEST_LIBRARY_LIST:
    dsc = os.path.join(SDK_LIBRARY_DIR, library, 'library.dsc')
    args.append(dsc)

  for example in TEST_EXAMPLE_LIST:
    dsc = os.path.join(SDK_LIBRARY_DIR, example, 'example.dsc')
    args.append(dsc)

  if build_experimental:
    args.append('--experimental')

  if generate_make.main(args):
    buildbot_common.ErrorExit('Failed to build tests.')


def BuildStepBuildTests(pepperdir, platform):
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'testlibs',
                             'Build Test Libraries')
  build_sdk.BuildStepMakeAll(pepperdir, platform, 'tests', 'Build Tests')


def BuildStepRunPyautoTests(pepperdir, platform, pepper_ver):
  buildbot_common.BuildStep('Test Examples')
  env = copy.copy(os.environ)
  env['PEPPER_VER'] = pepper_ver
  env['NACL_SDK_ROOT'] = pepperdir

  pyauto_script = os.path.join(SRC_DIR, 'chrome', 'test',
                               'functional', 'nacl_sdk.py')
  pyauto_script_args = ['nacl_sdk.NaClSDKTest.NaClSDKExamples']

  if platform == 'linux' and buildbot_common.IsSDKBuilder():
    # linux buildbots need to run the pyauto tests through xvfb. Running
    # using runtest.py does this.
    #env['PYTHON_PATH'] = '.:' + env.get('PYTHON_PATH', '.')
    build_dir = os.path.dirname(SRC_DIR)
    runtest_py = os.path.join(build_dir, '..', '..', '..', 'scripts', 'slave',
                              'runtest.py')
    buildbot_common.Run([sys.executable, runtest_py, '--target', 'Release',
                         '--build-dir', 'src/build', sys.executable,
                         pyauto_script] + pyauto_script_args,
                        cwd=build_dir, env=env)
  else:
    buildbot_common.Run([sys.executable, 'nacl_sdk.py',
                         'nacl_sdk.NaClSDKTest.NaClSDKExamples'],
                        cwd=os.path.dirname(pyauto_script),
                        env=env)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--experimental', help='build experimental tests',
                    action='store_true')
  parser.add_option('--pyauto', help='Run pyauto tests', action='store_true')

  options, args = parser.parse_args(args[1:])

  platform = getos.GetPlatform()
  pepper_ver = str(int(build_utils.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  toolchains = ['newlib', 'glibc', 'pnacl', 'host']

  BuildStepBuildExamples(pepperdir, platform)
  BuildStepCopyTests(pepperdir, toolchains, options.experimental, True)
  BuildStepBuildTests(pepperdir, platform)
  if options.pyauto:
    BuildStepRunPyautoTests(pepperdir, platform, pepper_ver)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
