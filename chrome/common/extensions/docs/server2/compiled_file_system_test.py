#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from compiled_file_system import CompiledFileSystem
from copy import deepcopy
from file_system import FileNotFoundError
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_object_store import TestObjectStore
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

def _CreateFactory():
  return CompiledFileSystem.Factory(
      TestFileSystem(deepcopy(_TEST_DATA)),
      ObjectStoreCreator.TestFactory(version='3-0', branch='test'))

class CompiledFileSystemTest(unittest.TestCase):
  def testIdentityNamespace(self):
    factory = _CreateFactory()
    compiled_fs = factory.CreateIdentity(CompiledFileSystemTest)
    self.assertEqual('3-0/CompiledFileSystem@test/CompiledFileSystemTest/file',
                     compiled_fs._file_object_store.namespace)

  def testIdentityFromFile(self):
    compiled_fs = _CreateFactory().CreateIdentity(CompiledFileSystemTest)
    self.assertEqual('404.html contents', compiled_fs.GetFromFile('404.html'))
    self.assertEqual('a11y.html contents',
                     compiled_fs.GetFromFile('apps/a11y.html'))
    self.assertEqual('file.html contents',
                     compiled_fs.GetFromFile('/apps/fakedir/file.html'))

  def testIdentityFromFileListing(self):
    compiled_fs = _CreateFactory().CreateIdentity(CompiledFileSystemTest)
    self.assertEqual(set(('404.html',
                          'apps/a11y.html',
                          'apps/about_apps.html',
                          'apps/fakedir/file.html',
                          'extensions/activeTab.html',
                          'extensions/alarms.html')),
                     set(compiled_fs.GetFromFileListing('/')))
    self.assertEqual(set(('a11y.html', 'about_apps.html', 'fakedir/file.html')),
                     set(compiled_fs.GetFromFileListing('apps/')))
    self.assertEqual(set(('file.html',)),
                     set(compiled_fs.GetFromFileListing('apps/fakedir')))

  def testPopulateNamespace(self):
    def CheckNamespace(expected_file, expected_list, fs):
      self.assertEqual(expected_file, fs._file_object_store.namespace)
      self.assertEqual(expected_list, fs._list_object_store.namespace)
    factory = _CreateFactory()
    f = lambda x: x
    CheckNamespace(
        '3-0/CompiledFileSystem@test/CompiledFileSystemTest/file',
        '3-0/CompiledFileSystem@test/CompiledFileSystemTest/list',
        factory.Create(f, CompiledFileSystemTest))
    CheckNamespace(
        '3-0/CompiledFileSystem@test/CompiledFileSystemTest/foo/file',
        '3-0/CompiledFileSystem@test/CompiledFileSystemTest/foo/list',
        factory.Create(f, CompiledFileSystemTest, category='foo'))

  def testPopulateFromFile(self):
    def Sleepy(key, val):
      return '%s%s' % ('Z' * len(key), 'z' * len(val))
    compiled_fs = _CreateFactory().Create(Sleepy, CompiledFileSystemTest)
    self.assertEqual('ZZZZZZZZzzzzzzzzzzzzzzzzz',
                     compiled_fs.GetFromFile('404.html'))
    self.assertEqual('ZZZZZZZZZZZZZZzzzzzzzzzzzzzzzzzz',
                     compiled_fs.GetFromFile('apps/a11y.html'))
    self.assertEqual('ZZZZZZZZZZZZZZZZZZZZZZZzzzzzzzzzzzzzzzzzz',
                     compiled_fs.GetFromFile('/apps/fakedir/file.html'))

  def testCaching(self):
    compiled_fs = _CreateFactory().CreateIdentity(CompiledFileSystemTest)
    self.assertEqual('404.html contents', compiled_fs.GetFromFile('404.html'))
    self.assertEqual(set(('file.html',)),
                     set(compiled_fs.GetFromFileListing('apps/fakedir')))

    compiled_fs._file_system._obj['404.html'] = 'boom'
    compiled_fs._file_system._obj['apps']['fakedir']['boom.html'] = 'blam'
    self.assertEqual('404.html contents', compiled_fs.GetFromFile('404.html'))
    self.assertEqual(set(('file.html',)),
                     set(compiled_fs.GetFromFileListing('apps/fakedir')))

    compiled_fs._file_system.IncrementStat()
    self.assertEqual('boom', compiled_fs.GetFromFile('404.html'))
    self.assertEqual(set(('file.html', 'boom.html')),
                     set(compiled_fs.GetFromFileListing('apps/fakedir')))

  def testFailures(self):
    compiled_fs = _CreateFactory().CreateIdentity(CompiledFileSystemTest)
    self.assertRaises(FileNotFoundError, compiled_fs.GetFromFile, '405.html')
    # TODO(kalman): would be nice to test this fails since apps/ is a dir.
    compiled_fs.GetFromFile('apps/')
    #self.assertRaises(SomeError, compiled_fs.GetFromFile, 'apps/')
    self.assertRaises(FileNotFoundError,
                      compiled_fs.GetFromFileListing, 'nodir/')
    # TODO(kalman): likewise, not a FileNotFoundError.
    self.assertRaises(FileNotFoundError,
                      compiled_fs.GetFromFileListing, '404.html')

if __name__ == '__main__':
  unittest.main()
