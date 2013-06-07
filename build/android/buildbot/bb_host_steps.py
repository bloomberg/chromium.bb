#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
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
  buildbot_report.PrintNamedStep('Check licenses for WebView')
  RunCmd([SrcPath('android_webview', 'tools', 'webview_licenses.py'), 'scan'],
         warning_code=1)


def RunHooks():
  buildbot_report.PrintNamedStep('runhooks')
  RunCmd(['gclient', 'runhooks'], halt_on_failure=True)


def Compile(build_type, args, experimental=False):
  cmd = [os.path.join(SLAVE_SCRIPTS_DIR, 'compile.py'),
         '--build-tool=ninja',
         '--compiler=goma',
         '--target=%s' % build_type,
         '--goma-dir=%s' % os.path.join(bb_utils.BB_BUILD_DIR, 'goma')]
  if experimental:
    for compile_target in args:
      buildbot_report.PrintNamedStep('Experimental Compile %s' % compile_target)
      RunCmd(cmd + ['--build-args=%s' % compile_target], flunk_on_failure=False)
  else:
    buildbot_report.PrintNamedStep('compile')
    RunCmd(cmd + ['--build-args=%s' % ' '.join(args)], halt_on_failure=True)


def ZipBuild(factory_properties, build_properties):
  buildbot_report.PrintNamedStep('Zip build')
  RunCmd([os.path.join(SLAVE_SCRIPTS_DIR, 'zip_build.py'),
          '--src-dir', constants.DIR_SOURCE_ROOT,
          '--build-dir', SrcPath('out'),
          '--exclude-files', 'lib.target,gen,android_webview,jingle_unittests',
          '--factory-properties', json.dumps(factory_properties),
          '--build-properties', json.dumps(build_properties)])


def ExtractBuild(factory_properties, build_properties):
  buildbot_report.PrintNamedStep('Download and extract build')
  RunCmd([os.path.join(SLAVE_SCRIPTS_DIR, 'extract_build.py'),
          '--build-dir', SrcPath('build'),
          '--build-output-dir', SrcPath('out'),
          '--factory-properties', json.dumps(factory_properties),
          '--build-properties', json.dumps(build_properties)],
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


def UpdateClang():
  RunCmd([SrcPath('tools', 'clang', 'scripts', 'update.sh')])


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
  parser.add_option('--update-clang', action='store_true',
                    help='Download or build the ASan runtime library.')

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
    RunHooks()
    Compile(build_type, options.build_args.split(','))
    if options.experimental:
      Compile(build_type, EXPERIMENTAL_TARGETS, True)
    if 'findbugs' in host_tests:
      FindBugs(build_type == 'Release')
    if options.zip_build:
      ZipBuild(options.factory_properties, options.build_properties)
  if options.update_clang:
    UpdateClang()
  if options.extract_build:
    ExtractBuild(options.factory_properties, options.build_properties)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
