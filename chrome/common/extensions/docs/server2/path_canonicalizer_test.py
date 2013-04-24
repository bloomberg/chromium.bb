#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from compiled_file_system import CompiledFileSystem
from path_canonicalizer import PathCanonicalizer
import svn_constants
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
import unittest

_TEST_DATA = TestFileSystem.MoveTo(svn_constants.PUBLIC_TEMPLATE_PATH, {
  'extensions': {
    'browserAction.html': 'yo',
    'storage.html': 'dawg',
  },
  'apps': {
    'bluetooth': 'hey',
    'storage.html': 'wassup',
  }
})

class PathCanonicalizerTest(unittest.TestCase):
  def setUp(self):
    test_fs = TestFileSystem(_TEST_DATA)
    compiled_fs_factory = CompiledFileSystem.Factory(
        test_fs,
        ObjectStoreCreator.TestFactory())
    self._path_canonicalizer = PathCanonicalizer('stable', compiled_fs_factory)

  def _assertIdentity(self, path):
    self.assertEqual(path, self._path_canonicalizer.Canonicalize(path))

  def testExtensions(self):
    self._assertIdentity('extensions/browserAction.html')
    self._assertIdentity('extensions/storage.html')
    self._assertIdentity('extensions/bluetooth.html')
    self._assertIdentity('extensions/blah.html')
    self._assertIdentity('stable/extensions/browserAction.html')
    self._assertIdentity('stable/extensions/storage.html')
    self._assertIdentity('stable/extensions/bluetooth.html')
    self._assertIdentity('stable/extensions/blah.html')

  def testApps(self):
    self._assertIdentity('apps/browserAction.html')
    self._assertIdentity('apps/storage.html')
    self._assertIdentity('apps/bluetooth.html')
    self._assertIdentity('apps/blah.html')
    self._assertIdentity('stable/apps/browserAction.html')
    self._assertIdentity('stable/apps/storage.html')
    self._assertIdentity('stable/apps/bluetooth.html')
    self._assertIdentity('stable/apps/blah.html')

  def testStatic(self):
    self._assertIdentity('static/browserAction.html')
    self._assertIdentity('static/storage.html')
    self._assertIdentity('static/bluetooth.html')
    self._assertIdentity('static/blah.html')
    self._assertIdentity('stable/static/browserAction.html')
    self._assertIdentity('stable/static/storage.html')
    self._assertIdentity('stable/static/bluetooth.html')
    self._assertIdentity('stable/static/blah.html')

  def testNeither(self):
    self.assertEqual(
        'extensions/browserAction.html',
        self._path_canonicalizer.Canonicalize('browserAction.html'))
    self.assertEqual(
        'stable/extensions/browserAction.html',
        self._path_canonicalizer.Canonicalize('stable/browserAction.html'))
    self.assertEqual(
        'extensions/storage.html',
        self._path_canonicalizer.Canonicalize('storage.html'))
    self.assertEqual(
        'stable/extensions/storage.html',
        self._path_canonicalizer.Canonicalize('stable/storage.html'))
    self.assertEqual(
        'apps/bluetooth.html',
        self._path_canonicalizer.Canonicalize('bluetooth.html'))
    self.assertEqual(
        'stable/apps/bluetooth.html',
        self._path_canonicalizer.Canonicalize('stable/bluetooth.html'))
    # Assign non-existent paths to extensions because they came first, so such
    # paths are more likely to be for extensions.
    self.assertEqual(
        'extensions/blah.html',
        self._path_canonicalizer.Canonicalize('blah.html'))
    self.assertEqual(
        'stable/extensions/blah.html',
        self._path_canonicalizer.Canonicalize('stable/blah.html'))

if __name__ == '__main__':
  unittest.main()
