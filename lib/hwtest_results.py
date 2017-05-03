# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes to manage HWTest result."""

from __future__ import print_function

import collections


HWTEST_RESULT_COLUMNS = ['id', 'build_id', 'test_name', 'status']

_hwTestResult = collections.namedtuple('_hwTestResult',
                                       HWTEST_RESULT_COLUMNS)


class HWTestResult(_hwTestResult):
  """Class to present HWTest status."""

  @classmethod
  def FromReport(cls, build_id, test_name, status):
    """Get a HWTestResult instance from a result report."""
    return HWTestResult(None, build_id, test_name, status)

  @classmethod
  def FromEntry(cls, entry):
    """Get a HWTestResult instance from cidb entry."""
    return HWTestResult(entry.id, entry.build_id, entry.test_name, entry.status)
