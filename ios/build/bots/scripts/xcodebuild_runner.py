# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runner for running tests using xcodebuild."""

import sys

import collections
import json
import multiprocessing
import os
import plistlib
import re
import shutil
import subprocess
import tempfile
import time

import test_runner


class LaunchCommandCreationError(test_runner.TestRunnerError):
  """One of launch command parameters was not set properly."""

  def __init__(self, message):
    super(LaunchCommandCreationError, self).__init__(message)


class LaunchCommandPoolCreationError(test_runner.TestRunnerError):
  """Failed to create a pool of launch commands."""

  def __init__(self, message):
    super(LaunchCommandPoolCreationError, self).__init__(message)


def test_status_summary(summary_plist):
  """Gets status summary from TestSummaries.plist.

  Args:
    summary_plist: (str) A path to plist-file.

  Returns:
    A dict that contains all passed and failed tests from each egtests.app.
    e.g.
    {
        'passed': {
            'egtests.app': [passed_tests]
        },
        'failed': {
            'egtests.app': [failed_tests]
        }
    }
  """
  root_summary = plistlib.readPlist(summary_plist)
  status_summary = {
      'passed': {},
      'failed': {}
  }
  for summary in root_summary['TestableSummaries']:
    failed_egtests = []
    passed_egtests = []
    for test_suite in summary['Tests'][0]['Subtests'][0]['Subtests']:
      for test in test_suite['Subtests']:
        if test['TestStatus'] == 'Success':
          passed_egtests.append(test['TestIdentifier'])
        else:
          failed_egtests.append(test['TestIdentifier'])
    module = summary['TargetName'].replace('_module', '.app')
    if failed_egtests:
      status_summary['failed'][module] = failed_egtests
    if passed_egtests:
      status_summary['passed'][module] = passed_egtests
  return status_summary


def collect_test_results(cmd_list, plist_path, return_code, output):
  """Gets test result data from Info.plist and from test output.

  Args:
    cmd_list: (list(str)) A running command.
    plist_path: (str) A path to plist-file.
    return_code: (int) A return code of cmd_list command.
    output: (str) An output of command.
  Returns:
    Test result as a map:
      {
        'passed': {
            'egtests.app': [passed_tests]
        },
        'failed': {
            'egtests.app': [failed_tests]
        },
        'output': ['Command output']
    }
  """
  test_results = {
      'passed': {},
      'failed': {},
      'errors': []
  }
  root = plistlib.readPlist(plist_path)

  for action in root['Actions']:
    summary_plist = os.path.join(os.path.dirname(plist_path),
                                 action['ActionResult']['TestSummaryPath'])
    summary = test_status_summary(summary_plist)
    test_results['failed'] = summary['failed']
    test_results['passed'] = summary['passed']

    for error_summary in action['ActionResult']['ErrorSummaries']:
      test_results['errors'].append(error_summary['Message'])
  # If xcodebuild finished with non-zero status and no failure/error
  # in the Info.plist log.
  if return_code and (
      not test_results['failed'] and not test_results['errors']):
    test_results['errors'].append(
        'Return code of "%s" is not 0!' % cmd_list)
  test_results['output'] = output
  return test_results


class EgtestsApp(object):
  """Egtests to run.

  Stores data about egtests:
    egtests_app: full path to egtests app.
    project_path: root project folder.
    module_name: egtests module name.
    filtered_tests: List of tests to include or exclude, depending on `invert`.
    invert: type of filter(True - inclusive, False - exclusive).
  """

  def __init__(self, egtests_app, filtered_tests=None, invert=False):
    """Initialize Egtests.

    Args:
      egtests_app: (str) full path to egtests app.
      filtered_tests: (list) Specific tests to run
        (it can inclusive/exclusive based on invert parameter).
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      invert: type of filter(True - inclusive, False - exclusive).

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    if not os.path.exists(egtests_app):
      raise test_runner.AppNotFoundError(egtests_app)
    self.egtests_path = egtests_app
    self.project_path = os.path.dirname(self.egtests_path)
    self.module_name = os.path.splitext(os.path.basename(egtests_app))[0]
    self.filter = filtered_tests
    self.invert = invert

  def _xctest_path(self):
    """Gets xctest-file from egtests/PlugIns folder.

    Returns:
      A path for xctest in the format of /PlugIns/file.xctest

    Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
    """
    plugins_dir = os.path.join(self.egtests_path, 'PlugIns')
    if not os.path.exists(plugins_dir):
      raise test_runner.PlugInsNotFoundError(plugins_dir)
    plugin_xctest = None
    for plugin in os.listdir(plugins_dir):
      if plugin.startswith(
          self.module_name) and plugin.endswith('.xctest'):
        plugin_xctest = os.path.join(plugins_dir, plugin)
    if not plugin_xctest:
      raise test_runner.XCTestPlugInNotFoundError(plugins_dir)
    return plugin_xctest.replace(self.egtests_path, '')

  def xctestrun_node(self):
    """Fills only required nodes for egtests in xctestrun file.

    Returns:
      A node with filled required fields about egtests.
    """
    xctestrun_data = {
        self.module_name + '_module': {
            'IsAppHostedTestBundle': True,
            'TestBundlePath': '__TESTHOST__%s' % self._xctest_path(),
            'TestHostPath': '%s' % self.egtests_path,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES': (
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                    'usr/lib/libXCTestBundleInject.dylib'),
                'DYLD_LIBRARY_PATH': self.project_path,
                'DYLD_FRAMEWORK_PATH': self.project_path + ':',
                'XCInjectBundleInto': '__TESTHOST__/%s' % self.module_name
            }
        }
    }
    if self.filter:
      if self.invert:
        xctestrun_data[self.module_name + '_module'].update(
            {'SkipTestIdentifiers': self.filter})
      else:
        xctestrun_data[self.module_name + '_module'].update(
            {'OnlyTestIdentifiers': self.filter})
    return xctestrun_data


class LaunchCommand(object):
  """Stores xcodebuild test launching command."""

  def __init__(self, egtests_app, destination,
               shards,
               retries,
               out_dir=os.path.basename(os.getcwd()),
               env=None):
    """Initialize launch command.

    Args:
      egtests_app: (EgtestsApp) An egtests_app to run.
      destination: (str) A destination.
      shards: (int) A number of shards.
      retries: (int) A number of retries.
      out_dir: (str) A folder in which xcodebuild will generate test output.
        By default it is a current directory.
      env: (dict) Environment variables.

    Raises:
      LaunchCommandCreationError: if one of parameters was not set properly.
    """
    if not isinstance(egtests_app, EgtestsApp):
      raise test_runner.AppNotFoundError(
          'Parameter `egtests_app` is not EgtestsApp: %s' % egtests_app)
    self.egtests_app = egtests_app
    self.destination = destination
    self.shards = shards
    self.retries = retries
    self.out_dir = out_dir
    self.logs = collections.OrderedDict()
    self.test_results = collections.OrderedDict()
    self.env = env

  def _make_cmd_list_for_failed_tests(self, failed_results, out_dir):
    """Makes cmd list based on failure results.

    Args:
      failed_results: Map of failed tests, where key is name of egtests_app and
        value is a list of failed_test_case/test_methods:
          {
              'egtests_app_name': [failed_test_case/test_methods]
          }
      out_dir: (str) An output path.

    Returns:
      List of Launch commands to re-run failed tests.
      Every destination will run on separate clone of a stimulator.
    """
    eg_app = EgtestsApp(
        egtests_app=self.egtests_app.egtests_path,
        filtered_tests=[test.replace(' ', '/') for test in
                        failed_results[os.path.basename(
                            self.egtests_app.egtests_path)]])
    # Regenerates xctest run and gets a command.
    return self.command(eg_app, out_dir, self.destination, shards=1)

  def _copy_screenshots(self, info_plist_path, output_folder):
    """Copy screenshots of failed tests to output folder.

    Args:
      info_plist_path: (str) A full path to Info.plist
      output_folder: (str) A full path to folder where
    """
    plist = plistlib.readPlist(info_plist_path)
    if 'TestFailureSummaries' not in plist or not plist['TestFailureSummaries']:
      print 'No failures in %s' % info_plist_path
      return

    screenshot_regex = re.compile(r'Screenshots:\s\{(\n.*)*\n}')
    for failure_summary in plist['TestFailureSummaries']:
      screenshots = screenshot_regex.search(failure_summary['Message'])
      test_case_folder = os.path.join(
          output_folder,
          'failures',
          failure_summary['TestCase'].replace('[', '').replace(']', '').replace(
              ' ', '_').replace('-', ''))
      if not os.path.exists(test_case_folder):
        os.makedirs(test_case_folder)
      if screenshots:
        print 'Screenshots for failure "%s" in "%s"' % (
            failure_summary['TestCase'], test_case_folder)
        d = json.loads(screenshots.group().replace('Screenshots:', '').strip())
        for f in d.values():
          if not os.path.exists(f):
            print 'File %s does not exist!' % f
            continue
          screenshot = os.path.join(test_case_folder, os.path.basename(f))
          print screenshot
          shutil.copyfile(f, screenshot)

  def summary_log(self):
    """Calculates test summary - how many passed, failed and error tests.

    Returns:
      Dictionary with number of errors, passed and failed tests.
      Failed tests will be calculated from the last test attempt.
      Passed tests and errors calculated for each test attempt.
    """
    test_statuses = ['errors', 'passed', 'failed']
    for status in test_statuses:
      self.logs[status] = 0

    for index, test_attempt_results in enumerate(self.test_results['attempts']):
      for test_status in test_statuses:
        if test_status not in test_attempt_results:
          continue
        results = test_attempt_results[test_status]
        if test_status == 'errors':
          self.logs['errors'] += len(results)
          continue
        for _, destinations_egtests in results.iteritems():
          if test_status == 'passed' or (
              # Number of failed tests is taken only from last run.
              test_status == 'failed' and index == len(
                  self.test_results['attempts']) - 1):
            self.logs[test_status] += len(destinations_egtests)

  def launch_attempt(self, cmd):
    """Launch a process and do logging simultaneously.

    Args:
      cmd: (list[str]) A command to run.

    Returns:
      A tuple (cmd, returncode, output) where:
      - returncode: return code of command run.
      - output: command output as list of strings.
    """
    print 'Launching %s with env %s' % (cmd, self.env)
    output = []
    proc = subprocess.Popen(
        cmd,
        env=self.env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    while True:
      line = proc.stdout.readline()
      if not line:
        break
      line = line.rstrip()
      print line
      output.append(line)
      sys.stdout.flush()

    proc.wait()
    print 'Command %s finished with %d' % (cmd, proc.returncode)
    return proc.returncode, output

  def launch(self):
    """Launches tests using xcodebuild."""
    initial_command = []
    self.test_results['attempts'] = []

    # total number of attempts is self.retries+1
    for attempt in range(self.retries + 1):
      outdir_attempt = os.path.join(self.out_dir, 'attempt_%d' % attempt)
      if attempt == 0:
        cmd_list = self.command(self.egtests_app,
                                outdir_attempt,
                                self.destination,
                                self.shards)
        initial_command = list(cmd_list)
      else:
        cmd_list = self._make_cmd_list_for_failed_tests(
            self.test_results['attempts'][-1]['failed'],
            outdir_attempt)
      # TODO(crbug.com/914878): add heartbeat logging to xcodebuild_runner.
      print 'Start test attempt #%d for command [%s]' % (
          attempt, ' '.join(cmd_list))
      return_code, output = self.launch_attempt(cmd_list)
      self.test_results['attempts'].append(
          collect_test_results(cmd_list, os.path.join(outdir_attempt,
                                                      'Info.plist'),
                               return_code, output))
      if self.retries == attempt or not self.test_results[
          'attempts'][-1]['failed']:
        break
      self._copy_screenshots(os.path.join(outdir_attempt, 'Info.plist'),
                             outdir_attempt)

    self.test_results['end_run'] = int(time.time())
    self.summary_log()

    return {
        'cmd': initial_command,
        'test_results': self.test_results,
        'logs': self.logs
    }

  def fill_xctest_run(self, egtests_app):
    """Fills xctestrun file by egtests.

    Args:
      egtests_app: (EgtestsApp) An Egetsts_app to run.

    Returns:
      A path to xctestrun file.

    Raises:
      AppNotFoundError if egtests is empty.
    """
    if not egtests_app:
      raise test_runner.AppNotFoundError('Egtests is not found!')
    xctestrun = tempfile.mkstemp()[1]
    # Creates a dict with data about egtests to run - fill all required fields:
    # egtests_module, egtest_app_path, egtests_xctest_path and
    # filtered tests if filter is specified.
    # Write data in temp xctest run file.
    plistlib.writePlist(egtests_app.xctestrun_node(), xctestrun)
    return xctestrun

  def command(self, egtests_app, out_dir, destination, shards):
    """Returns the command that launches tests using xcodebuild.

    Format of command:
    xcodebuild test-without-building -xctestrun file.xctestrun \
      -parallel-testing-enabled YES -parallel-testing-worker-count %d% \
      [-destination "destination"]  -resultBundlePath %output_path%

    Args:
      egtests_app: (EgtestsApp) An egetsts_app to run.
      out_dir: (str) An output directory.
      destination: (str) A destination of running simulator.
      shards: (int) A number of shards.

    Returns:
      A list of strings forming the command to launch the test.
    """
    return ['xcodebuild', 'test-without-building',
            '-xctestrun', self.fill_xctest_run(egtests_app),
            '-resultBundlePath', out_dir,
            '-parallel-testing-enabled', 'YES',
            '-destination', destination,
            '-parallel-testing-worker-count', str(shards)]


class SimulatorParallelTestRunner(test_runner.SimulatorTestRunner):
  """Class for running simulator tests using xCode."""

  def __init__(
      self,
      app,
      xcode_build_version,
      version,
      platform,
      out_dir,
      mac_toolchain=None,
      retry=1,
      shards=1,
      xcode_path=None
  ):
    """Initializes a new instance of SimulatorParallelTestRunner class.

    Args:
      app: (str) A path to egtests_app.
      xcode_build_version: (str) Xcode build version for running tests.
      version: (str) iOS version to run simulator on.
      platform: (str) Name of device.
      out_dir: (str) A directory to emit test data into.
      mac_toolchain: (str) A command to run `mac_toolchain` tool.
      retry: (int) A number to retry test run, will re-run only failed tests.
      shards: (int) A number of shards. Default is 1.
      xcode_path: (str) A path to Xcode.app folder.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    self.app = os.path.abspath(app)
    self.xcode_build_version = xcode_build_version
    self.version = version
    self.platform = platform
    self.out_dir = out_dir
    self.mac_toolchain = mac_toolchain
    self.retry = retry or 1
    self.shards = shards or 1
    self.xcode_path = xcode_path
    self._init_sharding_data()
    self.logs = collections.OrderedDict()
    self.test_results = collections.OrderedDict()
    self.test_results['start_run'] = int(time.time())
    self.test_results['end_run'] = None

  def _init_sharding_data(self):
    """Initialize sharding data.

    For common case info about sharding tests will be a list of dictionaries:
    [
        {
            'app':paths to egtests_app,
            'destination': 'platform=iOS Simulator,OS=<os>,Name=<simulator>'
            'shards': N
        }
    ]
    """
    self.sharding_data = [
        {
            'app': self.app,
            # Destination is required to run tests via xcodebuild and it
            # looks like
            # 'platform=iOS Simulator,OS=<os_version>,Name=<simulator-name>'
            # By default all tests runs on 'platform=iOS Simulator'.
            'destination': 'platform=iOS Simulator,OS=%s,name=%s' % (
                self.version, self.platform),
            'shards': self.shards,
        }
    ]

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(test_runner.SimulatorTestRunner, self).get_launch_env()
    env['NSUnbufferedIO'] = 'YES'
    return env

  def launch(self):
    """Launches tests using xcodebuild."""
    destinaion_folder = lambda dest: dest.replace(
        'platform=iOS Simulator,', '').replace(',name=', ' ').replace('OS=', '')
    launch_commands = [
        LaunchCommand(EgtestsApp(params['app']),
                      params['destination'],
                      shards=params['shards'],
                      retries=self.retry,
                      out_dir=os.path.join(
                          self.out_dir,
                          destinaion_folder(params['destination'])),
                      env=self.get_launch_env())
        for params in self.sharding_data]

    pool = multiprocessing.pool.ThreadPool(len(launch_commands))
    self.test_results['commands'] = []
    for result in pool.imap_unordered(LaunchCommand.launch, launch_commands):
      self.logs[' '.join(result['cmd'])] = result['test_results']
      self.test_results['commands'].append(
          {'cmd': ' '.join(result['cmd']), 'logs': result['logs']})

    self.test_results['end_run'] = int(time.time())
    # Test is failed if any error occurs during test run
    # or there are failures for last run.
    return not self.test_results['commands'][-1]['logs']['failed'] and all(
        [not r['logs']['errors'] for r in self.test_results['commands']])
