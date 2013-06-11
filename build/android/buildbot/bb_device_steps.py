#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import glob
import multiprocessing
import os
import shutil
import sys

import bb_utils

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import android_commands
from pylib import buildbot_report
from pylib import constants
from pylib.gtest import gtest_config

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'android_testrunner'))
import errors


CHROME_SRC = constants.DIR_SOURCE_ROOT

# Describes an instrumation test suite:
#   test: Name of test we're running.
#   apk: apk to be installed.
#   apk_package: package for the apk to be installed.
#   test_apk: apk to run tests on.
#   test_data: data folder in format destination:source.
I_TEST = collections.namedtuple('InstrumentationTest', [
    'name', 'apk', 'apk_package', 'test_apk', 'test_data', 'host_driven_root'])

INSTRUMENTATION_TESTS = dict((suite.name, suite) for suite in [
    I_TEST('ContentShell',
           'ContentShell.apk',
           'org.chromium.content_shell_apk',
           'ContentShellTest',
           'content:content/test/data/android/device_files',
           None),
    I_TEST('ChromiumTestShell',
           'ChromiumTestShell.apk',
           'org.chromium.chrome.testshell',
           'ChromiumTestShellTest',
           'chrome:chrome/test/data/android/device_files',
           constants.CHROMIUM_TEST_SHELL_HOST_DRIVEN_DIR),
    I_TEST('AndroidWebView',
           'AndroidWebView.apk',
           'org.chromium.android_webview.shell',
           'AndroidWebViewTest',
           'webview:android_webview/test/data/device_files',
           None),
    ])

VALID_TESTS = set(['chromedriver', 'ui', 'unit', 'webkit', 'webkit_layout'])

RunCmd = bb_utils.RunCmd


# multiprocessing map_async requires a top-level function for pickle library.
def RebootDeviceSafe(device):
  """Reboot a device, wait for it to start, and squelch timeout exceptions."""
  try:
    android_commands.AndroidCommands(device).Reboot(True)
  except errors.DeviceUnresponsiveError as e:
    return e


def RebootDevices():
  """Reboot all attached and online devices."""
  # Early return here to avoid presubmit dependence on adb,
  # which might not exist in this checkout.
  if bb_utils.TESTING:
    return
  devices = android_commands.GetAttachedDevices()
  print 'Rebooting: %s' % devices
  if devices:
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
    buildbot_report.PrintNamedStep(suite.name)
    cmd = ['build/android/run_tests.py', '-s', suite.name] + args
    if suite.is_suite_exe:
      cmd.append('--exe')
    RunCmd(cmd)

def RunBrowserTestSuite(options):
  """Manages an invocation of run_browser_tests.py.

  Args:
    options: options object.
  """
  args = ['--verbose', '--num_retries=1']
  if options.target == 'Release':
    args.append('--release')
  if options.asan:
    args.append('--tool=asan')
  buildbot_report.PrintNamedStep(constants.BROWSERTEST_SUITE_NAME)
  RunCmd(['build/android/run_browser_tests.py'] + args)

def RunChromeDriverTests():
  """Run all the steps for running chromedriver tests."""
  buildbot_report.PrintNamedStep('chromedriver_annotation')
  RunCmd(['chrome/test/chromedriver/run_buildbot_steps.py',
          '--android-package=%s' % constants.CHROMIUM_TEST_SHELL_PACKAGE])

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

  RunCmd(['build/android/adb_install_apk.py'] + args, halt_on_failure=True)


def RunInstrumentationSuite(options, test):
  """Manages an invocation of run_instrumentaiton_tests.py.

  Args:
    options: options object
    test: An I_TEST namedtuple
  """
  buildbot_report.PrintNamedStep('%s_instrumentation_tests' % test.name.lower())

  InstallApk(options, test)
  args = ['--test-apk', test.test_apk, '--test_data', test.test_data,
          '--verbose', '-I']
  if options.target == 'Release':
    args.append('--release')
  if options.asan:
    args.append('--tool=asan')
  if options.upload_to_flakiness_server:
    args.append('--flakiness-dashboard-server=%s' %
                constants.UPSTREAM_FLAKINESS_SERVER)
  if test.host_driven_root:
    args.append('--python_test_root=%s' % test.host_driven_root)

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
  cmd_args = [
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
        '--build-number', str(options.build_properties.get('buildnumber', '')),
        '--master-name', options.build_properties.get('mastername', ''),
        '--build-name', options.build_properties.get('buildername', ''),
        '--platform=chromium-android']

  for flag in 'test_results_server', 'driver_name', 'additional_drt_flag':
    if flag in options.factory_properties:
      cmd_args.extend(['--%s' % flag.replace('_', '-'),
                       options.factory_properties.get(flag)])

  for f in options.factory_properties.get('additional_expectations', []):
    cmd_args.extend(
        ['--additional-expectations=%s' % os.path.join(CHROME_SRC, *f)])

  # TODO(dpranke): Remove this block after
  # https://codereview.chromium.org/12927002/ lands.
  for f in options.factory_properties.get('additional_expectations_files', []):
    cmd_args.extend(
        ['--additional-expectations=%s' % os.path.join(CHROME_SRC, *f)])

  RunCmd(['webkit/tools/layout_tests/run_webkit_tests.py'] + cmd_args,
         flunk_on_failure=False)


def MainTestWrapper(options):
  # Restart adb to work around bugs, sleep to wait for usb discovery.
  RunCmd(['adb', 'kill-server'])
  RunCmd(['adb', 'start-server'])
  RunCmd(['sleep', '1'])

  # Spawn logcat monitor
  logcat_dir = os.path.join(CHROME_SRC, 'out/logcat')
  shutil.rmtree(logcat_dir, ignore_errors=True)
  bb_utils.SpawnCmd(['build/android/adb_logcat_monitor.py', logcat_dir])

  # Wait for logcat_monitor to pull existing logcat
  RunCmd(['sleep', '5'])

  # Provision devices
  buildbot_report.PrintNamedStep('provision_devices')
  if options.reboot:
    RebootDevices()
  RunCmd(['build/android/provision_devices.py', '-t', options.target])

  # Device check and alert emails
  buildbot_report.PrintNamedStep('device_status_check')
  RunCmd(['build/android/device_status_check.py'], halt_on_failure=True)

  if options.install:
    test_obj = INSTRUMENTATION_TESTS[options.install]
    InstallApk(options, test_obj, print_step=True)

  if 'chromedriver' in options.test_filter:
    RunChromeDriverTests()
  if 'unit' in options.test_filter:
    RunTestSuites(options, gtest_config.STABLE_TEST_SUITES)
  if 'ui' in options.test_filter:
    for test in INSTRUMENTATION_TESTS.itervalues():
      RunInstrumentationSuite(options, test)
  if 'webkit' in options.test_filter:
    RunTestSuites(options, [
        gtest_config.Apk('webkit_unit_tests'),
    ])
    RunWebkitLint(options.target)
  if 'webkit_layout' in options.test_filter:
    RunWebkitLayoutTests(options)

  if options.experimental:
    RunTestSuites(options, gtest_config.EXPERIMENTAL_TEST_SUITES)
    RunBrowserTestSuite(options)

  # Print logcat, kill logcat monitor
  buildbot_report.PrintNamedStep('logcat_dump')
  RunCmd(['build/android/adb_logcat_printer.py', logcat_dir])

  buildbot_report.PrintNamedStep('test_report')
  for report in glob.glob(
      os.path.join(CHROME_SRC, 'out', options.target, 'test_logs', '*.log')):
    RunCmd(['cat', report])
    os.remove(report)


def main(argv):
  parser = bb_utils.GetParser()
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
  parser.add_option('--upload-to-flakiness-server', action='store_true',
                    help='Upload the results to the flakiness dashboard.')
  parser.add_option(
      '--auto-reconnect', action='store_true',
      help='Push script to device which restarts adbd on disconnections.')
  options, args = parser.parse_args(argv[1:])

  if args:
    return sys.exit('Unused args %s' % args)

  unknown_tests = set(options.test_filter) - VALID_TESTS
  if unknown_tests:
    return sys.exit('Unknown tests %s' % list(unknown_tests))

  setattr(options, 'target', options.factory_properties.get('target', 'Debug'))

  MainTestWrapper(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
