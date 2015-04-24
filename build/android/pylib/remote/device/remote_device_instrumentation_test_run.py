# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run specific test on specific environment."""

import logging
import os
import tempfile

from pylib.base import base_test_result
from pylib.remote.device import remote_device_test_run
from pylib.utils import apk_helper


class RemoteDeviceInstrumentationTestRun(
    remote_device_test_run.RemoteDeviceTestRun):
  """Run instrumentation tests on a remote device."""

  #override
  def TestPackage(self):
    return self._test_instance.test_package

  #override
  def _TriggerSetUp(self):
    """Set up the triggering of a test run."""
    logging.info('Triggering test run.')
    self._AmInstrumentTestSetup(
        self._test_instance._apk_under_test, self._test_instance.test_apk,
        self._test_instance.test_runner, environment_variables={})

  #override
  def _ParseTestResults(self):
    logging.info('Parsing results from stdout.')
    r = base_test_result.TestRunResults()
    result_code, result_bundle, statuses = (
        self._test_instance.ParseAmInstrumentRawOutput(
            self._results['results']['output'].splitlines()))
    result = self._test_instance.GenerateTestResults(
        result_code, result_bundle, statuses, 0, 0)

    if isinstance(result, base_test_result.BaseTestResult):
      r.AddResult(result)
    elif isinstance(result, list):
      r.AddResults(result)
    else:
      raise Exception('Unexpected result type: %s' % type(result).__name__)

    return r
