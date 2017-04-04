# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing unit tests for failure_message_lib."""

from __future__ import print_function

from chromite.lib import constants
from chromite.lib import failure_message_lib


class StageFailureHelper(object):
  """Helper method to create StageFailure instances for test."""

  @staticmethod
  def CreateStageFailure(
      failure_id=1, build_stage_id=1, outer_failure_id=None,
      exception_type='StepFailure', exception_message='exception message',
      exception_category=constants.EXCEPTION_CATEGORY_UNKNOWN,
      extra_info=None, timestamp=None, stage_name='stage_1', board='board_1',
      stage_status=constants.BUILDER_STATUS_PASSED, build_id=1,
      master_build_id=None, builder_name='builder_name_1',
      waterfall='waterfall', build_number='build_number_1',
      build_config='config_1', build_status=constants.BUILDER_STATUS_PASSED,
      important=True, buildbucket_id='bb_id'):
    return failure_message_lib.StageFailure(
        failure_id, build_stage_id, outer_failure_id, exception_type,
        exception_message, exception_category, extra_info, timestamp,
        stage_name, board, stage_status, build_id, master_build_id,
        builder_name, waterfall, build_number, build_config, build_status,
        important, buildbucket_id)
