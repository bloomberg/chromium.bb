# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run specific test on specific environment."""

import logging
import os
import sys

from pylib import constants
from pylib.base import base_test_result
from pylib.remote.device import remote_device_test_run
from pylib.remote.device import remote_device_helper

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'requests', 'src'))
sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'appurify-python', 'src'))
import appurify.api
import appurify.utils

class RemoteDeviceUirobotRun(remote_device_test_run.RemoteDeviceTestRun):
  """Run uirobot tests on a remote device."""

  DEFAULT_RUNNER_TYPE = 'android_robot'

  def __init__(self, env, test_instance):
    """Constructor.

    Args:
      env: Environment the tests will run in.
      test_instance: The test that will be run.
    """
    super(RemoteDeviceUirobotRun, self).__init__(env, test_instance)

  def TestPackage(self):
    pass

  #override
  def _TriggerSetUp(self):
    """Set up the triggering of a test run."""
    self._app_id = self._UploadAppToDevice(self._test_instance.apk_under_test)
    if not self._env.runner_type:
      runner_type = self.DEFAULT_RUNNER_TYPE
      logging.info('Using default runner type: %s', self.DEFAULT_RUNNER_TYPE)
    else:
      runner_type = self._env.runner_type
    self._test_id = self._GetTestByName(runner_type)
    config_body = {'duration': self._test_instance.minutes}
    self._SetTestConfig(runner_type, config_body)

  #override
  def _ParseTestResults(self):
    # TODO(rnephew): Populate test results object.
    results = remote_device_test_run.TestRunResults()
    return results

