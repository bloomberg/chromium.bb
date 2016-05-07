# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runners for iOS."""

import argparse
import collections
import os
import shutil
import subprocess
import sys
import tempfile
import time

import find_xcode
import gtest_utils


class Error(Exception):
  """Base class for errors."""
  pass


class TestRunnerError(Error):
  """Base class for TestRunner-related errors."""
  pass


class AppLaunchError(TestRunnerError):
  """The app failed to launch."""
  pass


class AppNotFoundError(TestRunnerError):
  """The requested app was not found."""
  def __init__(self, app_path):
    super(AppNotFoundError, self).__init__(
        'App does not exist: %s' % app_path)


class SimulatorNotFoundError(TestRunnerError):
  """The given simulator binary was not found."""
  def __init__(self, iossim_path):
    super(SimulatorNotFoundError, self).__init__(
        'Simulator does not exist: %s' % iossim_path)


class XcodeVersionNotFound(TestRunnerError):
  """The requested version of Xcode was not found."""
  def __init__(self, xcode_version):
    super(XcodeVersionNotFoundError, self).__init__(
        'Xcode version not found: %s', xcode_version)


def get_kif_test_filter(tests, invert=False):
  """Returns the KIF test filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to GKIF_SCENARIO_FILTER.
  """
  # A pipe-separated list of test cases with the "KIF." prefix omitted.
  # e.g. NAME:a|b|c matches KIF.a, KIF.b, KIF.c.
  # e.g. -NAME:a|b|c matches everything except KIF.a, KIF.b, KIF.c.
  test_filter = '|'.join(test.split('KIF.', 1)[-1] for test in tests)
  if invert:
    return '-NAME:%s' % test_filter
  return 'NAME:%s' % test_filter


def get_gtest_filter(tests, invert=False):
  """Returns the GTest filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to --gtest_filter.
  """
  # A colon-separated list of tests cases.
  # e.g. a:b:c matches a, b, c.
  # e.g. -a:b:c matches everything except a, b, c.
  test_filter = ':'.join(test for test in tests)
  if invert:
    return '-%s' % test_filter
  return test_filter


class TestRunner(object):
  """Base class containing common functionality."""

  def __init__(self, app_path, xcode_version, out_dir, test_args=None):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app to run.
      xcode_version: Version of Xcode to use when running the test.
      out_dir: Directory to emit test data into.
      test_args: List of strings to pass as arguments to the test when
        launching.

    Raises:
      AppNotFoundError: If the given app does not exist.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
    """
    if not os.path.exists(app_path):
      raise AppNotFoundError(app_path)

    if not find_xcode.find_xcode(xcode_version)['found']:
      raise XcodeVersionNotFoundError(xcode_version)

    if not os.path.exists(out_dir):
      os.makedirs(out_dir)

    self.app_name = os.path.splitext(os.path.split(app_path)[-1])[0]
    self.app_path = app_path
    self.cfbundleid = subprocess.check_output([
        '/usr/libexec/PlistBuddy',
        '-c', 'Print:CFBundleIdentifier',
        os.path.join(app_path, 'Info.plist'),
    ]).rstrip()
    self.logs = collections.OrderedDict()
    self.out_dir = out_dir
    self.test_args = test_args or []

  def get_launch_command(self, test_filter=None, invert=False):
    """Returns the command that can be used to launch the test app.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
        match everything except the given test cases.

    Returns:
      A list of strings forming the command to launch the test.
    """
    raise NotImplementedError

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    raise NotImplementedError

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    raise NotImplementedError

  def screenshot_desktop(self):
    """Saves a screenshot of the desktop in the output directory."""
    subprocess.check_call([
        'screencapture',
        os.path.join(self.out_dir, 'desktop_%s.png' % time.time()),
    ])

  @staticmethod
  def _run(cmd):
    """Runs the specified command, parsing GTest output.

    Args:
      cmd: List of strings forming the command to run.

    Returns:
      GTestResult instance.
    """
    print ' '.join(cmd)
    print

    parser = gtest_utils.GTestLogParser()
    result = gtest_utils.GTestResult(cmd)

    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    while True:
      line = proc.stdout.readline()
      if not line:
        break
      line = line.rstrip()
      parser.ProcessLine(line)
      print line

    proc.wait()

    for test in parser.FailedTests(include_flaky=True):
      # Test cases are named as <test group>.<test case>. If the test case
      # is prefixed with "FLAKY_", it should be reported as flaked not failed.
      if '.' in test and test.split('.', 1)[1].startswith('FLAKY_'):
        result.flaked_tests[test] = parser.FailureDescription(test)
      else:
        result.failed_tests[test] = parser.FailureDescription(test)

    result.passed_tests.extend(parser.PassedTests(include_flaky=True))

    print '%s returned %s' % (cmd[0], proc.returncode)
    print

    # iossim can return 5 if it exits noncleanly even if all tests passed.
    # Therefore we cannot rely on process exit code to determine success.
    result.finalize(proc.returncode, parser.CompletedWithoutFailure())
    return result

  def launch(self):
    """Launches the test app."""
    self.set_up()
    cmd = self.get_launch_command()
    try:
      result = self._run(cmd)
      if result.crashed and not result.crashed_test:
        # If the app crashed but not during any particular test case, assume
        # it crashed on startup. Try one more time.
        print 'Crashed on startup, retrying...'
        print
        result = self._run(cmd)

      if result.crashed and not result.crashed_test:
        raise AppLaunchError

      passed = result.passed_tests
      failed = result.failed_tests
      flaked = result.flaked_tests

      try:
        while result.crashed and result.crashed_test:
          # If the app crashes during a specific test case, then resume at the
          # next test case. This is achieved by filtering out every test case
          # which has already run.
          print 'Crashed during %s, resuming...' % result.crashed_test
          print
          result = self._run(self.get_launch_command(
              test_filter=passed + failed.keys() + flaked.keys(), invert=True,
          ))
          passed.extend(result.passed_tests)
          failed.update(result.failed_tests)
          flaked.update(result.flaked_tests)
      except OSError as e:
        if e.errno == errno.E2BIG:
          print 'Too many test cases to resume.'
          print
        else:
          raise

      self.logs['passed tests'] = passed
      for test, log_lines in failed.iteritems():
        self.logs[test] = log_lines
      for test, log_lines in flaked.iteritems():
        self.logs[test] = log_lines

      return not failed
    finally:
      self.tear_down()


class SimulatorTestRunner(TestRunner):
  """Class for running tests on iossim."""

  def __init__(
      self,
      app_path,
      iossim_path,
      platform,
      version,
      xcode_version,
      out_dir,
      test_args=None,
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app or .ipa to run.
      iossim_path: Path to the compiled iossim binary to use.
      platform: Name of the platform to simulate. Supported values can be found
        by running "iossim -l". e.g. "iPhone 5s", "iPad Retina".
      version: Version of iOS the platform should be running. Supported values
        can be found by running "iossim -l". e.g. "9.3", "8.2", "7.1".
      xcode_version: Version of Xcode to use when running the test.
      out_dir: Directory to emit test data into.
      test_args: List of strings to pass as arguments to the test when
        launching.

    Raises:
      AppNotFoundError: If the given app does not exist.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
    """
    super(SimulatorTestRunner, self).__init__(
        app_path, xcode_version, out_dir, test_args=test_args)

    if not os.path.exists(iossim_path):
      raise SimulatorNotFoundError(iossim_path)

    self.homedir = ''
    self.iossim_path = iossim_path
    self.platform = platform
    self.start_time = None
    self.version = version

  @staticmethod
  def kill_simulators():
    """Kills all running simulators."""
    try:
      subprocess.check_call([
          'pkill',
          '-9',
          '-x',
          # The simulator's name varies by Xcode version.
          'iPhone Simulator', # Xcode 5
          'iOS Simulator', # Xcode 6
          'Simulator', # Xcode 7
      ])
    except subprocess.CalledProcessError as e:
      if e.returncode != 1:
        # Ignore a 1 exit code (which means there were no simulators to kill).
        raise

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.kill_simulators()
    self.homedir = tempfile.mkdtemp()
    # Crash reports have a timestamp in their file name, formatted as
    # YYYY-MM-DD-HHMMSS. Save the current time in the same format so
    # we can compare and fetch crash reports from this run later on.
    self.start_time = time.strftime('%Y-%m-%d-%H%M%S', time.localtime())

  def extract_test_data(self):
    """Extracts data emitted by the test."""
    # Find the directory named after the unique device ID of the simulator we
    # started. We expect only one because we use a new homedir each time.
    udid_dir = os.path.join(
        self.homedir, 'Library', 'Developer', 'CoreSimulator', 'Devices')
    if not os.path.exists(udid_dir):
      return
    udids = os.listdir(udid_dir)
    if len(udids) != 1:
      return

    # Find the Documents directory of the test app. The app directory names
    # don't correspond with any known information, so we have to examine them
    # all until we find one with a matching CFBundleIdentifier.
    apps_dir = os.path.join(
        udid_dir, udids[0], 'data', 'Containers', 'Data', 'Application')
    if os.path.exists(apps_dir):
      for appid_dir in os.listdir(apps_dir):
        docs_dir = os.path.join(apps_dir, appid_dir, 'Documents')
        metadata_plist = os.path.join(
            apps_dir,
            appid_dir,
            '.com.apple.mobile_container_manager.metadata.plist',
        )
        if os.path.exists(docs_dir) and os.path.exists(metadata_plist):
          cfbundleid = subprocess.check_output([
              '/usr/libexec/PlistBuddy',
              '-c', 'Print:MCMMetadataIdentifier',
              metadata_plist,
          ]).rstrip()
          if cfbundleid == self.cfbundleid:
            shutil.copytree(docs_dir, os.path.join(self.out_dir, 'Documents'))
            return

  def retrieve_crash_reports(self):
    """Retrieves crash reports produced by the test."""
    # A crash report's naming scheme is [app]_[timestamp]_[hostname].crash.
    # e.g. net_unittests_2014-05-13-15-0900_vm1-a1.crash.
    crash_reports_dir = os.path.expanduser(os.path.join(
        '~', 'Library', 'Logs', 'DiagnosticReports'))

    if not os.path.exists(crash_reports_dir):
      return

    for crash_report in os.listdir(crash_reports_dir):
      report_name, ext = os.path.splitext(crash_report)
      if report_name.startswith(self.app_name) and ext == '.crash':
        report_time = report_name[len(self.app_name) + 1:].split('_')[0]

        # The timestamp format in a crash report is big-endian and therefore
        # a staight string comparison works.
        if report_time > self.start_time:
          with open(os.path.join(crash_reports_dir, crash_report)) as f:
            self.logs['crash report (%s)' % report_time] = (
                f.read().splitlines())

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    self.extract_test_data()
    self.retrieve_crash_reports()
    self.screenshot_desktop()
    self.kill_simulators()
    if os.path.exists(self.homedir):
      shutil.rmtree(self.homedir, ignore_errors=True)
      self.homedir = ''

  def get_launch_command(self, test_filter=None, invert=False):
    """Returns the command that can be used to launch the test app.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
        match everything except the given test cases.

    Returns:
      A list of strings forming the command to launch the test.
    """
    cmd = [
        self.iossim_path,
        '-d', self.platform,
        '-s', self.version,
        '-t', '120',
        '-u', self.homedir,
    ]
    args = []

    if test_filter:
      kif_filter = get_kif_test_filter(test_filter, invert=invert)
      gtest_filter = get_gtest_filter(test_filter, invert=invert)
      cmd.extend(['-e', 'GKIF_SCENARIO_FILTER=%s' % kif_filter])
      args.append('--gtest_filter=%s' % gtest_filter)

    cmd.append(self.app_path)
    cmd.extend(self.test_args)
    cmd.extend(args)
    return cmd
