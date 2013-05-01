#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import deepcopy
from file_system import FileNotFoundError, StatInfo
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

class TestFileSystemTest(unittest.TestCase):
  def testEmptyFileSystem(self):
    self._TestMetasyntacticPaths(TestFileSystem({}))

  def testNonemptyFileNotFoundErrors(self):
    fs = TestFileSystem(deepcopy(_TEST_DATA))
    self._TestMetasyntacticPaths(fs)
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['404.html/'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['apps/foo/'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['apps/foo.html'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['apps/foo.html'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['apps/foo/',
                                                         'apps/foo.html'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['apps/foo/',
                                                         'apps/a11y.html'])

  def _TestMetasyntacticPaths(self, fs):
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['foo'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['bar/'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['bar/baz'])
    self.assertRaises(FileNotFoundError, _Get(fs.Read), ['foo',
                                                         'bar/',
                                                         'bar/baz'])
    self.assertRaises(FileNotFoundError, fs.Stat, 'foo')
    self.assertRaises(FileNotFoundError, fs.Stat, 'bar/')
    self.assertRaises(FileNotFoundError, fs.Stat, 'bar/baz')

  def testNonemptySuccess(self):
    fs = TestFileSystem(deepcopy(_TEST_DATA))
    self.assertEqual('404.html contents', fs.ReadSingle('404.html'))
    self.assertEqual('404.html contents', fs.ReadSingle('/404.html'))
    self.assertEqual('a11y.html contents', fs.ReadSingle('apps/a11y.html'))
    self.assertEqual(set(['404.html', 'apps/', 'extensions/']),
                     set(fs.ReadSingle('/')))
    self.assertEqual(set(['a11y.html', 'about_apps.html', 'fakedir/']),
                     set(fs.ReadSingle('apps/')))
    self.assertEqual(set(['a11y.html', 'about_apps.html', 'fakedir/']),
                     set(fs.ReadSingle('/apps/')))

  def testStat(self):
    fs = TestFileSystem(deepcopy(_TEST_DATA))
    self.assertRaises(FileNotFoundError, fs.Stat, 'foo')
    self.assertRaises(FileNotFoundError, fs.Stat, '404.html/')
    self.assertEquals(StatInfo('0'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('0', child_versions={
                        'activeTab.html': '0',
                        'alarms.html': '0',
                      }), fs.Stat('extensions/'))

    fs.IncrementStat()
    self.assertEquals(StatInfo('1'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('1', child_versions={
                        'activeTab.html': '1',
                        'alarms.html': '1',
                      }), fs.Stat('extensions/'))

    fs.IncrementStat(path='404.html')
    self.assertEquals(StatInfo('2'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('1', child_versions={
                        'activeTab.html': '1',
                        'alarms.html': '1',
                      }), fs.Stat('extensions/'))

    fs.IncrementStat()
    self.assertEquals(StatInfo('3'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('2', child_versions={
                        'activeTab.html': '2',
                        'alarms.html': '2',
                      }), fs.Stat('extensions/'))

    fs.IncrementStat(path='extensions/')
    self.assertEquals(StatInfo('3'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('3', child_versions={
                        'activeTab.html': '2',
                        'alarms.html': '2',
                      }), fs.Stat('extensions/'))

    fs.IncrementStat(path='extensions/alarms.html')
    self.assertEquals(StatInfo('3'), fs.Stat('404.html'))
    self.assertEquals(StatInfo('3', child_versions={
                        'activeTab.html': '2',
                        'alarms.html': '3',
                      }), fs.Stat('extensions/'))

  def testCheckAndReset(self):
    fs = TestFileSystem(deepcopy(_TEST_DATA))

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

  def testMoveTo(self):
    self.assertEqual({'foo': {'a': 'b', 'c': 'd'}},
                    TestFileSystem.MoveTo('foo', {'a': 'b', 'c': 'd'}))
    self.assertEqual({'foo': {'bar': {'a': 'b', 'c': 'd'}}},
                    TestFileSystem.MoveTo('foo/bar', {'a': 'b', 'c': 'd'}))
    self.assertEqual({'foo': {'bar': {'baz': {'a': 'b'}}}},
                    TestFileSystem.MoveTo('foo/bar/baz', {'a': 'b'}))

if __name__ == '__main__':
  unittest.main()
