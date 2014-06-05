#!/usr/bin/python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the archive_lib module."""

import logging
import multiprocessing
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.lib import parallel


class LocalBuilderStatusTest(cros_test_lib.TestCase):
  """Test the correctness of the various LocalBuilderStatus methods."""

  Results = results_lib.Results
  SUCCESS = results_lib.Results.SUCCESS

  def setUp(self):
    self.Results.Clear()

  def tearDown(self):
    self.Results.Clear()

  def testEmptyFail(self):
    """Sometimes there is no board-specific information."""
    self.Results.Record('Sync', self.SUCCESS)
    builder_status = metadata_lib.LocalBuilderStatus.Get()
    self.assertEqual({}, builder_status.board_status_map)
    self.assertEqual(constants.FINAL_STATUS_FAILED,
                     builder_status.GetBuilderStatus('lumpy-paladin'))
    self.assertEqual(constants.FINAL_STATUS_PASSED,
                     builder_status.GetBuilderStatus('master-paladin'))

  def testAllFail(self):
    """Failures in a shared stage cause specific stages to fail."""
    self.Results.Record('Sync', self.SUCCESS)
    self.Results.Record('BuildPackages', self.SUCCESS, board='lumpy')
    self.Results.Record('Completion', 'FAIL')
    builder_status = metadata_lib.LocalBuilderStatus.Get()
    self.assertEqual({'lumpy': constants.FINAL_STATUS_PASSED},
                     builder_status.board_status_map)
    self.assertEqual(constants.FINAL_STATUS_FAILED,
                     builder_status.GetBuilderStatus('lumpy-paladin'))
    self.assertEqual(constants.FINAL_STATUS_FAILED,
                     builder_status.GetBuilderStatus('master-paladin'))

  def testMixedFail(self):
    """Specific stages can pass/fail separately."""
    self.Results.Record('Sync', self.SUCCESS)
    self.Results.Record('BuildPackages [lumpy]', 'FAIL', board='lumpy')
    self.Results.Record('BuildPackages [stumpy]', self.SUCCESS, board='stumpy')
    builder_status = metadata_lib.LocalBuilderStatus.Get()
    self.assertEqual({'lumpy': constants.FINAL_STATUS_FAILED,
                      'stumpy': constants.FINAL_STATUS_PASSED},
                     builder_status.board_status_map)
    self.assertEqual(constants.FINAL_STATUS_FAILED,
                     builder_status.GetBuilderStatus('lumpy-paladin'))
    self.assertEqual(constants.FINAL_STATUS_PASSED,
                     builder_status.GetBuilderStatus('stumpy-paladin'))
    self.assertEqual(constants.FINAL_STATUS_FAILED,
                     builder_status.GetBuilderStatus('winky-paladin'))
    self.assertEqual(constants.FINAL_STATUS_PASSED,
                     builder_status.GetBuilderStatus('master-paladin'))


@cros_test_lib.NetworkTest()
class MetadataFetchTest(cros_test_lib.TestCase):
  """Test functions for fetching metadata from GS."""

  def testPaladinBuilder(self):
    bot, version = ('x86-mario-paladin', '5611.0.0')
    full_version = metadata_lib.FindLatestFullVersion(bot, version)
    self.assertEqual(full_version, 'R35-5611.0.0-rc2')
    metadata = metadata_lib.GetBuildMetadata(bot, full_version)
    metadata_dict = metadata._metadata_dict # pylint: disable=W0212
    self.assertEqual(metadata_dict['status']['status'], 'passed')


class MetadataTest(cros_test_lib.TestCase):
  """Tests the correctness of various metadata methods."""

  def testGetDict(self):
    starting_dict = {'key1': 1,
                     'key2': '2',
                     'cl_actions': [('a', 1), ('b', 2)]}
    metadata = metadata_lib.CBuildbotMetadata(starting_dict)
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

  def testMultiprocessSafety(self):
    m = multiprocessing.Manager()
    metadata = metadata_lib.CBuildbotMetadata(multiprocess_manager=m)
    key_dict = {'key1': 1, 'key2': 2}
    starting_dict = {'key1': 1,
                     'key2': '2',
                     'key3': key_dict,
                     'cl_actions': [('a', 1), ('b', 2)]}

    # Test that UpdateWithDict is process-safe
    parallel.RunParallelSteps([lambda: metadata.UpdateWithDict(starting_dict)])
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

    # Test that UpdateKeyDictWithDict is process-safe
    parallel.RunParallelSteps([lambda: metadata.UpdateKeyDictWithDict(
        'key3', key_dict)])
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

    # Test that RecordCLAction is process-safe
    fake_change = metadata_lib.GerritPatchTuple(12345, 1, False)
    fake_action = ('asdf,')
    parallel.RunParallelSteps([lambda: metadata.RecordCLAction(fake_change,
                                                               fake_action)])
    ending_dict = metadata.GetDict()
    # Assert that an action was recorded.
    self.assertEqual(len(starting_dict['cl_actions']) + 1,
                     len(ending_dict['cl_actions']))


if __name__ == '__main__':
  cros_test_lib.main(level=logging.DEBUG)
