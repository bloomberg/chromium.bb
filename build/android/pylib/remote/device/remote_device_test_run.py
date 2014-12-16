# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run specific test on specific environment."""

import logging
import os
import sys
import tempfile
import time

from pylib import constants
from pylib.base import test_run
from pylib.remote.device import appurify_sanitized
from pylib.remote.device import remote_device_helper

class RemoteDeviceTestRun(test_run.TestRun):
  """Run gtests and uirobot tests on a remote device."""

  WAIT_TIME = 5
  COMPLETE = 'complete'

  def __init__(self, env, test_instance):
    """Constructor.

    Args:
      env: Environment the tests will run in.
      test_instance: The test that will be run.
    """
    super(RemoteDeviceTestRun, self).__init__(env, test_instance)
    self._env = env
    self._test_instance = test_instance
    self._app_id = ''
    self._test_id = ''
    self._results = ''

  #override
  def RunTests(self):
    """Run the test."""
    if self._env.trigger:
      test_start_res = appurify_sanitized.api.tests_run(
          self._env.token, self._env.device, self._app_id, self._test_id)
      remote_device_helper.TestHttpResponse(
        test_start_res, 'Unable to run test.')
      test_run_id = test_start_res.json()['response']['test_run_id']
      logging.info('Test run id: %s' % test_run_id)
      if not self._env.collect:
        assert isinstance(self._env.trigger, basestring), (
                          'File for storing test_run_id must be a string.')
        with open(self._env.trigger, 'w') as test_run_id_file:
          test_run_id_file.write(test_run_id)

    if self._env.collect:
      if not self._env.trigger:
        assert isinstance(self._env.trigger, basestring), (
                          'File for storing test_run_id must be a string.')
        with open(self._env.collect, 'r') as test_run_id_file:
          test_run_id = test_run_id_file.read().strip()
      while self._GetTestStatus(test_run_id) != self.COMPLETE:
        time.sleep(self.WAIT_TIME)
      self._DownloadTestResults(self._env.results_path)
      return self._ParseTestResults()

  #override
  def TearDown(self):
    """Tear down the test run."""
    pass

  def __enter__(self):
    """Set up the test run when used as a context manager."""
    self.SetUp()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Tear down the test run when used as a context manager."""
    self.TearDown()

  #override
  def SetUp(self):
    """Set up a test run."""
    if self._env.trigger:
      self._TriggerSetUp()

  def _TriggerSetUp(self):
    """Set up the triggering of a test run."""
    raise NotImplementedError

  def _ParseTestResults(self):
    raise NotImplementedError

  def _GetTestByName(self, test_name):
    """Gets test_id for specific test.

    Args:
      test_name: Test to find the ID of.
    """
    test_list_res = appurify_sanitized.api.tests_list(self._env.token)
    remote_device_helper.TestHttpResponse(test_list_res,
                                          'Unable to get tests list.')
    for test in test_list_res.json()['response']:
      if test['test_type'] == test_name:
        return test['test_id']
    raise remote_device_helper.RemoteDeviceError(
        'No test found with name %s' % (test_name))

  def _DownloadTestResults(self, results_path):
    """Download the test results from remote device service.

    Args:
      results_path: path to download results to.
    """
    if results_path:
      logging.info('Downloading results to %s.' % results_path)
      if not os.path.exists(os.path.basename(results_path)):
        os.makedirs(os.path.basename(results_path))
      appurify_sanitized.utils.wget(self._results['results']['url'],
                                    results_path)

  def _GetTestStatus(self, test_run_id):
    """Checks the state of the test, and sets self._results

    Args:
      test_run_id: Id of test on on remote service.
    """

    test_check_res = appurify_sanitized.api.tests_check_result(self._env.token,
                                                     test_run_id)
    remote_device_helper.TestHttpResponse(test_check_res,
                                          'Unable to get test status.')
    self._results = test_check_res.json()['response']
    logging.info('Test status: %s' % self._results['detailed_status'])
    return self._results['status']

  def _UploadAppToDevice(self, apk_path):
    """Upload app to device."""
    logging.info('Upload %s to remote service.' % apk_path)
    apk_name = os.path.basename(apk_path)
    with open(apk_path, 'rb') as apk_src:
      upload_results = appurify_sanitized.api.apps_upload(self._env.token,
                                                apk_src, 'raw', name=apk_name)
      remote_device_helper.TestHttpResponse(
          upload_results, 'Unable to upload %s.' %(apk_path))
      return upload_results.json()['response']['app_id']

  def _UploadTestToDevice(self, test_type):
    """Upload test to device
    Args:
      test_type: Type of test that is being uploaded. Ex. uirobot, gtest..
    """
    logging.info('Uploading %s to remote service.' % self._test_instance.apk)
    with open(self._test_instance.apk, 'rb') as test_src:
      upload_results = appurify_sanitized.api.tests_upload(
          self._env.token, test_src, 'raw', test_type, app_id=self._app_id)
      remote_device_helper.TestHttpResponse(upload_results,
          'Unable to upload %s.' %(self._test_instance.apk))
      return upload_results.json()['response']['test_id']

  def _SetTestConfig(self, runner_type, body):
    """Generates and uploads config file for test.
    Args:
      extras: Extra arguments to set in the config file.
    """
    logging.info('Generating config file for test.')
    with tempfile.TemporaryFile() as config:
      config_data = ['[appurify]', '[%s]' % runner_type]
      config_data.extend('%s=%s' % (k, v) for k, v in body.iteritems())
      config.write(''.join('%s\n' % l for l in config_data))
      config.flush()
      config.seek(0)
      config_response = appurify_sanitized.api.config_upload(self._env.token,
                                                   config, self._test_id)
      remote_device_helper.TestHttpResponse(config_response,
                                            'Unable to upload test config.')
