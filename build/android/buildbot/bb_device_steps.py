#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import glob
import json
import multiprocessing
import optparse
import os
import pipes
import shutil
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import android_commands
from pylib import buildbot_report
from pylib import constants
from pylib.gtest import gtest_config

sys.path.append(os.path.join(
    constants.CHROME_DIR, 'third_party', 'android_testrunner'))
import errors


TESTING = 'BUILDBOT_TESTING' in os.environ

CHROME_SRC = constants.CHROME_DIR

# Describes an instrumation test suite:
#   test: Name of test we're running.
#   apk: apk to be installed.
#   apk_package: package for the apk to be installed.
#   test_apk: apk to run tests on.
#   test_data: data folder in format destination:source.
I_TEST = collections.namedtuple('InstrumentationTest', [
    'name', 'apk', 'apk_package', 'test_apk', 'test_data'])

INSTRUMENTATION_TESTS = dict((suite.name, suite) for suite in [
    I_TEST('ContentShell',
           'ContentShell.apk',
           'org.chromium.content_shell',
           'ContentShellTest',
           'content:content/test/data/android/device_files'),
    I_TEST('ChromiumTestShell',
           'ChromiumTestShell.apk',
           'org.chromium.chrome.testshell',
           'ChromiumTestShellTest',
           'chrome:chrome/test/data/android/device_files'),
    I_TEST('AndroidWebView',
           'AndroidWebView.apk',
           'org.chromium.android_webview',
           'AndroidWebViewTest',
           'webview:android_webview/test/data/device_files'),
    ])

VALID_TESTS = set(['ui', 'unit', 'webkit', 'webkit_layout'])



def SpawnCmd(command):
  """Spawn a process without waiting for termination."""
  print '>', ' '.join(map(pipes.quote, command))
  sys.stdout.flush()
  if TESTING:
    class MockPopen(object):
      @staticmethod
      def wait():
        return 0
    return MockPopen()

  return subprocess.Popen(command, cwd=CHROME_SRC)

def RunCmd(command, flunk_on_failure=True):
  """Run a command relative to the chrome source root."""
  code = SpawnCmd(command).wait()
  print '<', ' '.join(map(pipes.quote, command))
  if code != 0:
    print 'ERROR: process exited with code %d' % code
    if flunk_on_failure:
      buildbot_report.PrintError()
    else:
      buildbot_report.PrintWarning()
  return code


# multiprocessing map_async requires a top-level function for pickle library.
def RebootDeviceSafe(device):
  """Reboot a device, wait for it to start, and squelch timeout exceptions."""
  try:
    android_commands.AndroidCommands(device).Reboot(True)
  except errors.DeviceUnresponsiveError as e:
    return e


def RebootDevices():
  """Reboot all attached and online devices."""
  buildbot_report.PrintNamedStep('Reboot devices')
  devices = android_commands.GetAttachedDevices()
  print 'Rebooting: %s' % devices
  if devices and not TESTING:
    pool = multiprocessing.Pool(len(devices))
    results = pool.map_async(RebootDeviceSafe, devices).get(99999)

    for device, result in zip(devices, results):
      if result:
        print '%s failed to startup.' % device

    if any(results):
      buildbot_report.PrintWarning()
    else:
      print 'Reboots complete.'


def RunTestSuites(options, suites):
  """Manages an invocation of run_tests.py.

  Args:
    options: options object.
    suites: List of suites to run.
  """
  args = ['--verbose']
  if options.target == 'Release':
    args.append('--release')
  if options.asan:
    args.append('--tool=asan')
  for suite in suites:
    buildbot_report.PrintNamedStep(suite)
    RunCmd(['build/android/run_tests.py', '-s', suite] + args)


def InstallApk(options, test, print_step=False):
  """Install an apk to all phones.

  Args:
    options: options object
    test: An I_TEST namedtuple
    print_step: Print a buildbot step
  """
  if print_step:
    buildbot_report.PrintNamedStep('install_%s' % test.name.lower())
  args = ['--apk', test.apk, '--apk_package', test.apk_package]
  if options.target == 'Release':
    args.append('--release')

  RunCmd(['build/android/adb_install_apk.py'] + args)


def RunInstrumentationSuite(options, test):
  """Manages an invocation of run_instrumentaiton_tests.py.

  Args:
    options: options object
    test: An I_TEST namedtuple
  """
  buildbot_report.PrintNamedStep('%s_instrumentation_tests' % test.name.lower())

  InstallApk(options, test)
  args = ['--test-apk', test.test_apk, '--test_data', test.test_data, '-vvv',
          '-I']
  if options.target == 'Release':
    args.append('--release')
  if options.asan:
    args.append('--tool=asan')

  RunCmd(['build/android/run_instrumentation_tests.py'] + args)


def RunWebkitLint(target):
  """Lint WebKit's TestExpectation files."""
  buildbot_report.PrintNamedStep('webkit_lint')
  RunCmd(['webkit/tools/layout_tests/run_webkit_tests.py',
          '--lint-test-files',
          '--chromium',
          '--target', target])


def RunWebkitLayoutTests(options):
  """Run layout tests on an actual device."""
  buildbot_report.PrintNamedStep('webkit_tests')
  RunCmd(['webkit/tools/layout_tests/run_webkit_tests.py',
          '--no-show-results',
          '--no-new-test-results',
          '--full-results-html',
          '--clobber-old-results',
          '--exit-after-n-failures', '5000',
          '--exit-after-n-crashes-or-timeouts', '100',
          '--debug-rwt-logging',
          '--results-directory', '..layout-test-results',
          '--target', options.target,
          '--builder-name', options.build_properties.get('buildername', ''),
          '--build-number', options.build_properties.get('buildnumber', ''),
          '--master-name', options.build_properties.get('mastername', ''),
          '--build-name', options.build_properties.get('buildername', ''),
          '--platform=chromium-android',
          '--test-results-server',
          options.factory_properties.get('test_results_server', '')])


def MainTestWrapper(options):
  # Restart adb to work around bugs, sleep to wait for usb discovery.
  RunCmd(['adb', 'kill-server'])
  RunCmd(['adb', 'start-server'])
  RunCmd(['sleep', '1'])

  # Spawn logcat monitor
  logcat_dir = os.path.join(CHROME_SRC, 'out/logcat')
  shutil.rmtree(logcat_dir, ignore_errors=True)
  SpawnCmd(['build/android/adb_logcat_monitor.py', logcat_dir])

  # Wait for logcat_monitor to pull existing logcat
  RunCmd(['sleep', '5'])

  if options.reboot:
    RebootDevices()

  # Device check and alert emails
  buildbot_report.PrintNamedStep('device_status_check')
  RunCmd(['build/android/device_status_check.py'], flunk_on_failure=False)

  if options.install:
    test_obj = INSTRUMENTATION_TESTS[options.install]
    InstallApk(options, test_obj, print_step=True)

  if 'unit' in options.test_filter:
    RunTestSuites(options, gtest_config.STABLE_TEST_SUITES)
  if 'ui' in options.test_filter:
    for test in INSTRUMENTATION_TESTS.itervalues():
      RunInstrumentationSuite(options, test)
  if 'webkit' in options.test_filter:
    RunTestSuites(options, ['webkit_unit_tests', 'TestWebKitAPI'])
    RunWebkitLint(options.target)
  if 'webkit_layout' in options.test_filter:
    RunWebkitLayoutTests(options)

  if options.experimental:
    RunTestSuites(options, gtest_config.EXPERIMENTAL_TEST_SUITES)

  # Print logcat, kill logcat monitor
  buildbot_report.PrintNamedStep('logcat_dump')
  RunCmd(['build/android/adb_logcat_printer.py', logcat_dir])

  buildbot_report.PrintNamedStep('test_report')
  for report in glob.glob(
      os.path.join(CHROME_SRC, 'out', options.target, 'test_logs', '*.log')):
    RunCmd(['cat', report])
    os.remove(report)


def main(argv):
  parser = optparse.OptionParser()

  def convert_json(option, _, value, parser):
    setattr(parser.values, option.dest, json.loads(value))

  parser.add_option('--build-properties', action='callback',
                    callback=convert_json, type='string', default={},
                    help='build properties in JSON format')
  parser.add_option('--factory-properties', action='callback',
                    callback=convert_json, type='string', default={},
                    help='factory properties in JSON format')
  parser.add_option('--slave-properties', action='callback',
                    callback=convert_json, type='string', default={},
                    help='Properties set by slave script in JSON format')
  parser.add_option('--experimental', action='store_true',
                    help='Run experiemental tests')
  parser.add_option('-f', '--test-filter', metavar='<filter>', default=[],
                    action='append',
                    help=('Run a test suite. Test suites: "%s"' %
                          '", "'.join(VALID_TESTS)))
  parser.add_option('--asan', action='store_true', help='Run tests with asan.')
  parser.add_option('--install', metavar='<apk name>',
                    help='Install an apk by name')
  parser.add_option('--reboot', action='store_true',
                    help='Reboot devices before running tests')
  options, args = parser.parse_args(argv[1:])

  def ParserError(msg):
    """We avoid parser.error because it calls sys.exit."""
    parser.print_help()
    print >> sys.stderr, '\nERROR:', msg
    return 1

  if args:
    return ParserError('Unused args %s' % args)

  unknown_tests = set(options.test_filter) - VALID_TESTS
  if unknown_tests:
    return ParserError('Unknown tests %s' % list(unknown_tests))

  setattr(options, 'target', options.factory_properties.get('target', 'Debug'))

  # Add adb binary and chromium-source platform-tools to tip of PATH variable.
  android_paths = [os.path.join(constants.ANDROID_SDK_ROOT, 'platform-tools')]

  # Bots checkout chrome in /b/build/slave/<name>/build/src
  build_internal_android = os.path.abspath(os.path.join(
      CHROME_SRC, '..', '..', '..', '..', '..', 'build_internal', 'scripts',
      'slave', 'android'))
  if os.path.exists(build_internal_android):
    android_paths.insert(0, build_internal_android)
  os.environ['PATH'] = os.pathsep.join(android_paths + [os.environ['PATH']])

  MainTestWrapper(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
