#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from intro_data_source import IntroDataSource
from server_instance import ServerInstance
from servlet import Request
from test_data.canned_data import CANNED_TEST_FILE_SYSTEM_DATA
from test_file_system import TestFileSystem
import unittest

class IntroDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._server_instance = ServerInstance.ForTest(
        TestFileSystem(CANNED_TEST_FILE_SYSTEM_DATA))

  def testIntro(self):
    intro_data_source = IntroDataSource(
        self._server_instance, Request.ForTest(''))
    intro_data = intro_data_source.get('test_intro')
    article_data = intro_data_source.get('test_article')

    expected_intro = 'you<h2>first</h2><h3>inner</h3><h2>second</h2>'
    # Article still has the header.
    expected_article = '<h1>hi</h1>' + expected_intro

    self.assertEqual(expected_intro, intro_data.Render().text)
    self.assertEqual(expected_article, article_data.Render().text)


if __name__ == '__main__':
  unittest.main()
