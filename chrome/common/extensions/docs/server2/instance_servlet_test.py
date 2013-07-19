#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from empty_dir_file_system import EmptyDirFileSystem
from file_system import FileSystem
from instance_servlet import InstanceServlet
from servlet import Request
from test_branch_utility import TestBranchUtility
from test_util import DisableLogging

# XXX(kalman): what is this test supposed to be?
# Create a test host file system creator which failz?

# NOTE(kalman): The ObjectStore created by the InstanceServlet is backed onto
# our fake AppEngine memcache/datastore, so the tests aren't isolated.
class _TestDelegate(InstanceServlet.Delegate):
  def __init__(self, file_system_type):
    self._file_system_type = file_system_type

  def CreateBranchUtility(self, object_store_creator):
    return TestBranchUtility.CreateWithCannedData()

  def CreateAppSamplesFileSystem(self, object_store_creator):
    return EmptyDirFileSystem()

class _FailOnAccessFileSystem(FileSystem):
  # All this needs to do is implement GetIdentity. All other methods will
  # automatically fail with NotImplementedErrors.
  def GetIdentity(self):
    return '42'

class InstanceServletTest(unittest.TestCase):
  @DisableLogging('warning')
  def testHostFileSystemNotAccessed(self):
    delegate = _TestDelegate(_FailOnAccessFileSystem)
    constructor = InstanceServlet.GetConstructor(delegate_for_test=delegate)
    def test_path(path, status=404):
      response = constructor(Request.ForTest(path)).Get()
      self.assertEqual(status, response.status)
    test_path('extensions/storage.html')
    test_path('apps/storage.html')
    test_path('extensions/examples/foo.zip')
    test_path('extensions/examples/foo.html')
    test_path('static/foo.css')
    test_path('beta/extensions/storage.html', status=301)
    test_path('beta/apps/storage.html', status=301)
    test_path('beta/extensions/examples/foo.zip', status=301)
    test_path('beta/extensions/examples/foo.html', status=301)
    test_path('beta/static/foo.css', status=301)

if __name__ == '__main__':
  unittest.main()
