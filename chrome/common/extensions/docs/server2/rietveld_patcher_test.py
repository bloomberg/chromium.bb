#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from appengine_url_fetcher import AppEngineUrlFetcher
from fake_fetchers import ConfigureFakeFetchers
from file_system import FileNotFoundError
from rietveld_patcher import RietveldPatcher
from svn_constants import EXTENSIONS_PATH
import url_constants

class RietveldPatcherTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()
    self._patcher = RietveldPatcher(
        EXTENSIONS_PATH,
        '14096030',
        AppEngineUrlFetcher(url_constants.CODEREVIEW_SERVER))

  def _ReadLocalFile(self, filename):
    with open(os.path.join(sys.path[0],
                           'test_data',
                           'rietveld_patcher',
                           'expected',
                           filename), 'r') as f:
      return f.read()

  def _ApplySingle(self, path, binary=False):
    return self._patcher.Apply([path], None, binary).Get()[path]

  def testGetVersion(self):
    self.assertEqual(self._patcher.GetVersion(), '22002')

  def testGetPatchedFiles(self):
    added, deleted, modified = self._patcher.GetPatchedFiles()
    self.assertEqual(sorted(added),
                     sorted([
        u'docs/templates/articles/test_foo.html',
        u'docs/templates/public/extensions/test_foo.html']))
    self.assertEqual(sorted(deleted),
                     sorted([
        u'docs/templates/public/extensions/runtime.html']))
    self.assertEqual(sorted(modified),
                     sorted([
        u'api/test.json',
        u'docs/templates/json/extensions_sidenav.json']))

  def testApply(self):
    article_path = 'docs/templates/articles/test_foo.html'

    # Make sure RietveldPatcher handles |binary| correctly.
    self.assertTrue(isinstance(self._ApplySingle(article_path, True), str),
        'Expected result is binary. It was text.')
    self.assertTrue(isinstance(self._ApplySingle(article_path), unicode),
        'Expected result is text. It was binary.')

    # Apply to an added file.
    self.assertEqual(
        self._ReadLocalFile('test_foo.html'),
        self._ApplySingle(
            'docs/templates/public/extensions/test_foo.html'))

    # Apply to a modified file.
    self.assertEqual(
        self._ReadLocalFile('extensions_sidenav.json'),
        self._ApplySingle(
            'docs/templates/json/extensions_sidenav.json'))

    # Applying to a deleted file doesn't throw exceptions. It just returns
    # empty content.
    # self.assertRaises(FileNotFoundError, self._ApplySingle,
    #     'docs/templates/public/extensions/runtime.html')

    # Apply to an unknown file.
    self.assertRaises(FileNotFoundError, self._ApplySingle, 'not_existing')

if __name__ == '__main__':
  unittest.main()
