#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import deepcopy
from file_system import FileNotFoundError, StatInfo
from mock_file_system import MockFileSystem
from test_file_system import TestFileSystem
import unittest

_TEST_DATA = {
  '404.html': '404.html contents',
  'apps': {
    'a11y.html': 'a11y.html contents',
    'about_apps.html': 'about_apps.html contents',
    'fakedir': {
      'file.html': 'file.html contents'
    }
  },
  'extensions': {
    'activeTab.html': 'activeTab.html contents',
    'alarms.html': 'alarms.html contents'
  }
}

def _Get(fn):
  '''Returns a function which calls Future.Get on the result of |fn|.
  '''
  return lambda *args: fn(*args).Get()

class MockFileSystemTest(unittest.TestCase):
  def testCheckAndReset(self):
    fs = MockFileSystem(TestFileSystem(deepcopy(_TEST_DATA)))

    self.assertTrue(*fs.CheckAndReset())
    self.assertFalse(*fs.CheckAndReset(read_count=1))
    self.assertFalse(*fs.CheckAndReset(stat_count=1))

    fs.ReadSingle('apps/')
    self.assertTrue(*fs.CheckAndReset(read_count=1))
    self.assertFalse(*fs.CheckAndReset(read_count=1))
    self.assertTrue(*fs.CheckAndReset())

    fs.ReadSingle('apps/')
    self.assertFalse(*fs.CheckAndReset(read_count=2))

    fs.ReadSingle('extensions/')
    fs.ReadSingle('extensions/')
    self.assertTrue(*fs.CheckAndReset(read_count=2))
    self.assertFalse(*fs.CheckAndReset(read_count=2))
    self.assertTrue(*fs.CheckAndReset())

    fs.ReadSingle('404.html')
    fs.Read(['notfound.html', 'apps/'])
    self.assertTrue(*fs.CheckAndReset(read_count=2))

    fs.Stat('404.html')
    fs.Stat('404.html')
    fs.Stat('apps/')
    self.assertFalse(*fs.CheckAndReset(stat_count=42))
    self.assertFalse(*fs.CheckAndReset(stat_count=42))
    self.assertTrue(*fs.CheckAndReset())

    fs.ReadSingle('404.html')
    fs.Stat('404.html')
    fs.Stat('apps/')
    self.assertTrue(*fs.CheckAndReset(read_count=1, stat_count=2))
    self.assertTrue(*fs.CheckAndReset())

if __name__ == '__main__':
  unittest.main()
