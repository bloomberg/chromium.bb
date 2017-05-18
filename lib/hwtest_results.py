# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes to manage HWTest result."""

from __future__ import print_function

import collections

from chromite.lib import constants
from chromite.lib import cros_logging as logging

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

  @classmethod
  def NormalizeTestName(cls, test_name):
    """Normalize test name.

    Normalization examples:
    Suite job -> None
    cheets_CTS.com.android.cts.dram -> cheets_CTS
    security_NetworkListeners -> security_NetworkListeners

    Args:
      test_name: The test name string to normalize.

    Returns:
      Test name after normalization.
    """
    if test_name == 'Suite job':
      return None

    names = test_name.split('.')
    return names[0]

class HWTestResultManager(object):
  """Class to manage HWTest results."""

  @classmethod
  def GetHWTestResultsFromCIDB(cls, db, build_ids, test_statues=None):
    """Get HWTest results for given builds from CIDB.

    Args:
      db: An instance of cidb.CIDBConnection.
      build_ids: A list of build_ids (strings) to get HWTest results.
      test_statues: A set of HWTest statuses (stirngs) to get. If not None,
        only return HWTests in test_statues.

    Returns:
      A list of HWTestResult instances.
    """
    results = db.GetHWTestResultsForBuilds(build_ids)

    if test_statues is None:
      return results

    return [x for x in results if x.status in test_statues]

  @classmethod
  def GetFailedHWTestsFromCIDB(cls, db, build_ids):
    """Get test names of failed HWTests from CIDB.

    Args:
      db: An instance of cidb.CIDBConnection
      build_ids: A list of build_ids (strings) to get failed HWTests.

    Returns:
      A list of normalized HWTest names (strings).
    """
    # TODO: probably only count 'fail' and exclude abort' and 'other' results?
    hwtest_results = cls.GetHWTestResultsFromCIDB(
        db, build_ids, test_statues=constants.HWTEST_STATUES_NOT_PASSED)

    failed_tests = set([HWTestResult.NormalizeTestName(result.test_name)
                        for result in hwtest_results])
    failed_tests.discard(None)

    logging.info('Found failed tests: %s ', failed_tests)
    return failed_tests
