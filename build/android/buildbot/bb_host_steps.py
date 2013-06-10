#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import bb_utils

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import buildbot_report
from pylib import constants


SLAVE_SCRIPTS_DIR = os.path.join(bb_utils.BB_BUILD_DIR, 'scripts', 'slave')
VALID_HOST_TESTS = set(['check_webview_licenses', 'findbugs'])
EXPERIMENTAL_TARGETS = ['android_experimental']

# Short hand for RunCmd which is used extensively in this file.
RunCmd = bb_utils.RunCmd


def SrcPath(*path):
  return os.path.join(constants.DIR_SOURCE_ROOT, *path)


def CheckWebViewLicenses():
  buildbot_report.PrintNamedStep('check_licenses')
  RunCmd([SrcPath('android_webview', 'tools', 'webview_licenses.py'), 'scan'],
         warning_code=1)


def RunHooks(build_type):
  RunCmd([SrcPath('build', 'landmines.py')])
  build_path = SrcPath('out', build_type)
  landmine_path = os.path.join(build_path, '.landmines_triggered')
  clobber_env = os.environ.get('BUILDBOT_CLOBBER')
  if clobber_env or os.path.isfile(landmine_path):
    buildbot_report.PrintNamedStep('Clobber')
    if not clobber_env:
      print 'Clobbering due to triggered landmines:'
      with open(landmine_path) as f:
        print f.read()
    RunCmd(['rm', '-rf', build_path])

  buildbot_report.PrintNamedStep('runhooks')
  RunCmd(['gclient', 'runhooks'], halt_on_failure=True)


def Compile(build_type, args, experimental=False):
  cmd = [os.path.join(SLAVE_SCRIPTS_DIR, 'compile.py'),
         '--build-tool=ninja',
         '--compiler=goma',
         '--target=%s' % build_type,
         '--goma-dir=%s' % bb_utils.GOMA_DIR]
  if experimental:
    for compile_target in args:
      buildbot_report.PrintNamedStep('Experimental Compile %s' % compile_target)
      RunCmd(cmd + ['--build-args=%s' % compile_target], flunk_on_failure=False)
  else:
    buildbot_report.PrintNamedStep('compile')
    RunCmd(cmd + ['--build-args=%s' % ' '.join(args)], halt_on_failure=True)


def ZipBuild(properties):
  buildbot_report.PrintNamedStep('zip_build')
  RunCmd([
      os.path.join(SLAVE_SCRIPTS_DIR, 'zip_build.py'),
      '--src-dir', constants.DIR_SOURCE_ROOT,
      '--build-dir', SrcPath('out'),
      '--exclude-files', 'lib.target,gen,android_webview,jingle_unittests']
      + properties)


def ExtractBuild(properties):
  buildbot_report.PrintNamedStep('extract_build')
  RunCmd([os.path.join(SLAVE_SCRIPTS_DIR, 'extract_build.py'),
          '--build-dir', SrcPath('build'),
          '--build-output-dir', SrcPath('out')] + properties,
         warning_code=1)


def FindBugs(is_release):
  buildbot_report.PrintNamedStep('findbugs')
  build_type = []
  if is_release:
    build_type = ['--release-build']
  RunCmd([SrcPath('build', 'android', 'findbugs_diff.py')] + build_type)
  RunCmd([SrcPath(
      'tools', 'android', 'findbugs_plugin', 'test',
      'run_findbugs_plugin_tests.py')] + build_type)


def main(argv):
  parser = bb_utils.GetParser()
  parser.add_option('--host-tests', help='Comma separated list of host tests.')
  parser.add_option('--build-args', default='All',
                    help='Comma separated list of build targets.')
  parser.add_option('--compile', action='store_true',
                    help='Indicate whether a compile step should be run.')
  parser.add_option('--experimental', action='store_true',
                    help='Indicate whether to compile experimental targets.')
  parser.add_option('--zip-build', action='store_true',
                    help='Indicate whether the build should be zipped.')
  parser.add_option('--extract-build', action='store_true',
                    help='Indicate whether a build should be downloaded.')

  options, args = parser.parse_args(argv[1:])
  if args:
    return sys.exit('Unused args %s' % args)

  host_tests = []
  if options.host_tests:
    host_tests = options.host_tests.split(',')
  unknown_tests = set(host_tests) - VALID_HOST_TESTS
  if unknown_tests:
    return sys.exit('Unknown host tests %s' % list(unknown_tests))

  build_type = options.factory_properties.get('target', 'Debug')

  if options.compile:
    if 'check_webview_licenses' in host_tests:
      CheckWebViewLicenses()
    RunHooks(build_type)
    Compile(build_type, options.build_args.split(','))
    if options.experimental:
      Compile(build_type, EXPERIMENTAL_TARGETS, True)
    if 'findbugs' in host_tests:
      FindBugs(build_type == 'Release')
    if options.zip_build:
      ZipBuild(bb_utils.EncodeProperties(options))
  if options.extract_build:
    ExtractBuild(bb_utils.EncodeProperties(options))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
