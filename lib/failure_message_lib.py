# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage stage failure messages."""

from __future__ import print_function

import collections


# These keys must exist as column names from failureView in cidb.
FAILURE_KEYS = (
    'id', 'build_stage_id', 'outer_failure_id', 'exception_type',
    'exception_message', 'exception_category', 'extra_info',
    'timestamp', 'stage_name', 'board', 'stage_status', 'build_id',
    'master_build_id', 'builder_name', 'waterfall', 'build_number',
    'build_config', 'build_status', 'important', 'buildbucket_id')


# A namedtuple containing values fetched from CIDB failureView.
StageFailure = collections.namedtuple('StageFailure', FAILURE_KEYS)
