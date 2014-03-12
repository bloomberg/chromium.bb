#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Host-driven Java tests which exercise sync functionality."""

from pylib import constants
from pylib.host_driven import test_case
from pylib.host_driven import test_server
from pylib.host_driven import tests_annotations


class SyncTest(test_case.HostDrivenTestCase):
  """Host-driven Java tests which exercise sync functionality."""

  def __init__(self, *args, **kwargs):
    super(SyncTest, self).__init__(*args, **kwargs)
    self.test_server = None
    self.additional_flags = []

  def SetUp(self, device, shard_index, push_deps, cleanup_test_files):
    self.test_server = test_server.TestServer(
        shard_index,
        constants.TEST_SYNC_SERVER_PORT,
        test_server.TEST_SYNC_SERVER_PATH)
    # These ought not to change in the middle of a test for obvious reasons.
    self.additional_flags = [
        '--sync-url=http://%s:%d/chromiumsync' %
        (self.test_server.host, self.test_server.port),
        '--sync-deferred-startup-timeout-seconds=0']
    super(SyncTest, self).SetUp(device, shard_index, push_deps,
                                cleanup_test_files,
                                [self.test_server.port])

  def TearDown(self):
    super(SyncTest, self).TearDown()
    self.test_server.TearDown()

  def _RunSyncTests(self, test_names):
    full_names = []
    for test_name in test_names:
      full_names.append('SyncTest.' + test_name)
    return self._RunJavaTestFilters(full_names, self.additional_flags)

  # http://crbug.com/348951
  # @tests_annotations.Feature(['Sync'])
  # @tests_annotations.EnormousTest
  @tests_annotations.DisabledTest
  def testGetAboutSyncInfoYieldsValidData(self):
    java_tests = ['testGetAboutSyncInfoYieldsValidData']
    return self._RunSyncTests(java_tests)

  # http://crbug.com/348117
  # @tests_annotations.Feature(['Sync'])
  # @tests_annotations.EnormousTest
  @tests_annotations.DisabledTest
  def testAboutSyncPageDisplaysCurrentSyncStatus(self):
    java_tests = ['testAboutSyncPageDisplaysCurrentSyncStatus']
    return self._RunSyncTests(java_tests)

  # http://crbug.com/348951
  # @tests_annotations.Feature(['Sync'])
  # @tests_annotations.EnormousTest
  @tests_annotations.DisabledTest
  def testDisableAndEnableSync(self):
    java_tests = ['testDisableAndEnableSync']
    return self._RunSyncTests(java_tests)
