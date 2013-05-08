#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from cron_servlet import CronServlet
from empty_dir_file_system import EmptyDirFileSystem
from local_file_system import LocalFileSystem
from mock_file_system import MockFileSystem
from servlet import Request
from test_branch_utility import TestBranchUtility
from test_file_system import TestFileSystem
from test_util import EnableLogging

# NOTE(kalman): The ObjectStore created by the CronServlet is backed onto our
# fake AppEngine memcache/datastore, so the tests aren't isolated.
class _TestDelegate(CronServlet.Delegate):
  def __init__(self):
    self.host_file_systems = []

  def CreateBranchUtility(self, object_store_creator):
    return TestBranchUtility()

  def CreateHostFileSystemForBranch(self, branch):
    host_file_system = MockFileSystem(LocalFileSystem.Create())
    self.host_file_systems.append(host_file_system)
    return host_file_system

  def CreateAppSamplesFileSystem(self, object_store_creator):
    return EmptyDirFileSystem()

class CronServletTest(unittest.TestCase):
  @EnableLogging('info')
  def testEverything(self):
    # All these tests are dependent (see above comment) so lump everything in
    # the one test.
    delegate = _TestDelegate()

    # Test that the cron runs successfully.
    response = CronServlet(Request.ForTest('trunk'),
                           delegate_for_test=delegate).Get()
    self.assertEqual(1, len(delegate.host_file_systems))
    self.assertEqual(200, response.status)

    # When re-running, all file systems should be Stat()d the same number of
    # times, but the second round shouldn't have been re-Read() since the
    # Stats haven't changed.
    response = CronServlet(Request.ForTest('trunk'),
                           delegate_for_test=delegate).Get()
    self.assertEqual(2, len(delegate.host_file_systems))
    self.assertTrue(*delegate.host_file_systems[1].CheckAndReset(
        read_count=0,
        stat_count=delegate.host_file_systems[0].GetStatCount()))


if __name__ == '__main__':
  unittest.main()
