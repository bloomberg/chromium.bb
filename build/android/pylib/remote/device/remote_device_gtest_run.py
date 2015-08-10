# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run specific test on specific environment."""

import logging
import os
import re
import tempfile

from pylib import constants
from pylib.base import base_test_result
from pylib.remote.device import remote_device_test_run
from pylib.remote.device import remote_device_helper


_EXTRA_COMMAND_LINE_FILE = (
    'org.chromium.native_test.NativeTestActivity.CommandLineFile')
_NATIVE_CRASH_RE = re.compile('native crash', re.IGNORECASE)


class RemoteDeviceGtestTestRun(remote_device_test_run.RemoteDeviceTestRun):
  """Run gtests and uirobot tests on a remote device."""

  DEFAULT_RUNNER_PACKAGE = (
      'org.chromium.native_test.NativeTestInstrumentationTestRunner')

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def _TriggerSetUp(self):
    """Set up the triggering of a test run."""
    logging.info('Triggering test run.')

    if self._env.runner_type:
      logging.warning('Ignoring configured runner_type "%s"',
                      self._env.runner_type)

    if not self._env.runner_package:
      runner_package = self.DEFAULT_RUNNER_PACKAGE
      logging.info('Using default runner package: %s',
                   self.DEFAULT_RUNNER_PACKAGE)
    else:
      runner_package = self._env.runner_package

    dummy_app_path = os.path.join(
        constants.GetOutDirectory(), 'apks', 'remote_device_dummy.apk')
    with tempfile.NamedTemporaryFile(suffix='.flags.txt') as flag_file:
      env_vars = {}
      filter_string = self._test_instance._GenerateDisabledFilterString(None)
      if filter_string:
        flag_file.write('_ --gtest_filter=%s' % filter_string)
        flag_file.flush()
        env_vars[_EXTRA_COMMAND_LINE_FILE] = os.path.basename(flag_file.name)
        self._test_instance._data_deps.append(
            (os.path.abspath(flag_file.name), None))
      self._AmInstrumentTestSetup(
          dummy_app_path, self._test_instance.apk, runner_package,
          environment_variables=env_vars)

  _INSTRUMENTATION_STREAM_LEADER = 'INSTRUMENTATION_STATUS: stream='

  #override
  def _ParseTestResults(self):
    logging.info('Parsing results from stdout.')
    results = base_test_result.TestRunResults()
    output = self._results['results']['output'].splitlines()
    output = (l[len(self._INSTRUMENTATION_STREAM_LEADER):] for l in output
              if l.startswith(self._INSTRUMENTATION_STREAM_LEADER))
    results_list = self._test_instance.ParseGTestOutput(output)
    results.AddResults(results_list)
    if self._env.only_output_failures:
      logging.info('See logcat for more results information.')
    if not self._results['results']['pass']:
      if any(_NATIVE_CRASH_RE.search(l)
          for l in self._results['results']['output'].splitlines()):
        logging.critical('Native crash detected. Printing Logcat.')
        self._LogLogcat()
        results.AddResult(base_test_result.BaseTestResult(
            'Remote Service detected native crash.',
            base_test_result.ResultType.CRASH))
      elif self._DidDeviceGoOffline():
        self._LogLogcat()
        self._LogAdbTraceLog()
        raise remote_device_helper.RemoteDeviceError(
            'Remote service unable to reach device.', is_infra_error=True)
      else:
        results.AddResult(base_test_result.BaseTestResult(
            'Remote Service detected error.',
            base_test_result.ResultType.UNKNOWN))

    return results
