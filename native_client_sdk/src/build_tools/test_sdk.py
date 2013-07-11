#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for a testing an existing SDK.

This script is normally run immediately after build_sdk.py.
"""

import optparse
import os
import subprocess
import sys
import time

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

# TODO(binji): ugly hack -- can I get the browser in a cleaner way?
sys.path.append(os.path.join(SRC_DIR, 'chrome', 'test', 'nacl_test_injection'))
import find_chrome
browser_path = find_chrome.FindChrome(SRC_DIR, ['Debug', 'Release'])


def StepBuildExamples(pepperdir):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, 'examples',
                               'Build Examples (%s)' % config,
                               deps=False, config=config)


def StepCopyTests(pepperdir, toolchains, build_experimental):
  buildbot_common.BuildStep('Copy Tests')

  # Update test libraries and test apps
  filters = {
    'DEST': ['tests']
  }
  if not build_experimental:
    filters['EXPERIMENTAL'] = False

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  build_projects.UpdateHelpers(pepperdir, clobber=False)
  build_projects.UpdateProjects(pepperdir, tree, clobber=False,
                                toolchains=toolchains)


def StepBuildTests(pepperdir):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, 'tests',
                                   'Build Tests (%s)' % config,
                                   deps=False, config=config)


def StepRunSelLdrTests(pepperdir):
  filters = {
    'SEL_LDR': True
  }

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)

  def RunTest(test, toolchain, arch, config):
    args = ['TOOLCHAIN=%s' % toolchain, 'NACL_ARCH=%s' % arch]
    args += ['SEL_LDR=1', 'run']
    build_projects.BuildProjectsBranch(pepperdir, test, clean=False,
                                       deps=False, config=config,
                                       args=args)

  if getos.GetPlatform() == 'win':
    # On win32 we only support running on the system
    # arch
    archs = (getos.GetSystemArch('win'),)
  elif getos.GetPlatform() == 'mac':
    # We only ship 32-bit version of sel_ldr on mac.
    archs = ('x86_32',)
  else:
    # On linux we can run both 32 and 64-bit
    archs = ('x86_64', 'x86_32')

  for root, projects in tree.iteritems():
    for project in projects:
      title = 'sel_ldr tests: %s' % os.path.basename(project['NAME'])
      location = os.path.join(root, project['NAME'])
      buildbot_common.BuildStep(title)
      for toolchain in ('newlib', 'glibc'):
        for arch in archs:
          for config in ('Debug', 'Release'):
            RunTest(location, toolchain, arch, config)


def StepRunBrowserTests(pepperdir, toolchains, experimental):
  buildbot_common.BuildStep('Run Tests')

  platform = getos.GetPlatform()

  filters = {
    'DEST': [
      'examples/api',
      'examples/demo',
      'examples/getting_started',
      'examples/tutorial',
      'tests'
    ]
  }
  if not experimental:
    filters['EXPERIMENTAL'] = False

  if not browser_path:
    buildbot_common.ErrorExit('Can\'t find Chrome. Did you build it?')

  browser_tester_py = os.path.join(SRC_DIR, 'ppapi', 'native_client', 'tools',
      'browser_tester', 'browser_tester.py')

  # browser_tester imports tools/valgrind/memcheck_analyze, which imports
  # tools/valgrind/common. Well, it tries to, anyway, but instead imports
  # common from PYTHONPATH first (which on the buildbots, is a
  # common/__init__.py file...).
  #
  # Clear the PYTHONPATH so it imports the correct file.
  env = dict(os.environ)
  env['PYTHONPATH'] = ''

  failures = []
  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  for dest, desc in parse_dsc.GenerateProjects(tree):
    name = desc['NAME']
    path = os.path.join(pepperdir, *dest.split('/'))
    path = os.path.join(path, name)

    for toolchain in desc['TOOLS']:
      if toolchain not in toolchains:
        continue

      # TODO(binji): Make a better way to disable tests.
      if name == 'graphics_3d' and platform in ('linux', 'win'):
        continue

      for config in desc.get('CONFIGS', ['Debug', 'Release']):
        args = [
          sys.executable,
          browser_tester_py,
          '--browser_path', browser_path,
          '--timeout', '30.0',  # seconds
          # Prevent the infobar that shows up when requesting filesystem quota.
          '--browser_flag', '--unlimited-storage',
        ]

        args.extend(['--serving_dir', path])
        out_dir = os.path.join(path, toolchain, config)

        if toolchain == platform:
          ppapi_plugin = os.path.join(out_dir, name)
          if platform == 'win':
            ppapi_plugin += '.dll'
          else:
            ppapi_plugin += '.so'
          args.extend(['--ppapi_plugin', ppapi_plugin])

        if toolchain == 'pnacl':
          args.extend(['--browser_flag', '--enable-pnacl'])

        url = 'index.html'
        url += '?tc=%s&config=%s&test=true' % (toolchain, config)
        args.extend(['--url', url])

        # Fake gtest-like output
        test_name = '%s.%s_%s_test' % (name, toolchain, config.lower())

        print '\n[ RUN      ] %s' % test_name
        sys.stdout.flush()
        sys.stderr.flush()

        # Don't use buildbot_common.Run here -- we don't wait to exit if the
        # command fails.
        failed = False
        start_time = time.time()
        try:
          subprocess.check_call(args, env=env)
        except subprocess.CalledProcessError:
          failed = True
          failures.append(test_name)
        elapsed = (time.time() - start_time) * 1000

        sys.stdout.flush()
        sys.stderr.flush()
        if failed:
          message = '[  FAILED  ]'
        else:
          message = '[       OK ]'
        print '%s %s (%d ms)' % (message, test_name, elapsed)

  if failures:
    print '-' * 80
    print 'TEST FAILURES'
    print '-' * 80
    for failure in failures:
      print '  %s failed.' % failure
    buildbot_common.ErrorExit('Error running tests.')


def main(args):
  usage = '%prog [<options>] [<phase...>]'
  parser = optparse.OptionParser(description=__doc__, usage=usage)
  parser.add_option('--experimental', help='build experimental tests',
                    action='store_true')
  parser.add_option('--verbose', help='Verbose output', action='store_true')

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # of the build.
    del os.environ['NACL_SDK_ROOT']

  options, args = parser.parse_args(args[1:])

  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  toolchains = ['newlib', 'glibc', 'pnacl']
  toolchains.append(getos.GetPlatform())

  if options.verbose:
    build_projects.verbose = True

  phases = [
    ('build_examples', StepBuildExamples, pepperdir),
    ('copy_tests', StepCopyTests, pepperdir, toolchains, options.experimental),
    ('build_tests', StepBuildTests, pepperdir),
    ('sel_ldr_tests', StepRunSelLdrTests, pepperdir),
    ('browser_tests', StepRunBrowserTests, pepperdir, toolchains,
                      options.experimental),
  ]

  if args:
    phase_names = [p[0] for p in phases]
    for arg in args:
      if arg not in phase_names:
        msg = 'Invalid argument: %s\n' % arg
        msg += 'Possible arguments:\n'
        for name in phase_names:
          msg += '   %s\n' % name
        parser.error(msg.strip())

  for phase in phases:
    phase_name = phase[0]
    if args and phase_name not in args:
      continue
    phase_func = phase[1]
    phase_args = phase[2:]
    phase_func(*phase_args)

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('test_sdk: interrupted')
