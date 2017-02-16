# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for builder_status_lib."""

from __future__ import print_function

from chromite.lib import builder_status_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib


# pylint: disable=protected-access
class BuilderStatusManagerTest(cros_test_lib.MockTestCase):
  """Tests for BuilderStatusManager."""

  def testUnpickleBuildStatus(self):
    """Tests that _UnpickleBuildStatus returns the correct values."""
    failed_msg = failures_lib.BuildFailureMessage(
        'you failed', ['traceback'], True, 'taco', 'bot')
    failed_input_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, failed_msg)
    passed_input_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)

    failed_output_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(
            failed_input_status.AsPickledDict()))
    passed_output_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(
            passed_input_status.AsPickledDict()))
    empty_string_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(''))

    self.assertEqual(failed_input_status.AsFlatDict(),
                     failed_output_status.AsFlatDict())
    self.assertEqual(passed_input_status.AsFlatDict(),
                     passed_output_status.AsFlatDict())
    self.assertTrue(empty_string_status.Failed())
