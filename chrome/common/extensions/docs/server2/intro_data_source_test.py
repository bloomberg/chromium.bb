#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from intro_data_source import IntroDataSource
from server_instance import ServerInstance
from test_data.canned_data import CANNED_TEST_FILE_SYSTEM_DATA
from test_file_system import TestFileSystem
import unittest

class IntroDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._server_instance = ServerInstance.ForTest(
        TestFileSystem(CANNED_TEST_FILE_SYSTEM_DATA))

  def testIntro(self):
    intro_data_source = IntroDataSource(self._server_instance, None)
    intro_data = intro_data_source.get('test_intro')
    article_data = intro_data_source.get('test_article')

    self.assertEqual('hi', article_data.get('title'))
    self.assertEqual(None, intro_data.get('title'))

    # TODO(kalman): test links.
    expected_toc = [{
        'link': '',
        'subheadings': [{'link': '', 'subheadings': [], 'title': u'inner'}],
        'title': u'first',
      }, {
        'link': '',
        'subheadings': [],
        'title': u'second'
      }
    ]
    self.assertEqual(expected_toc, article_data.get('toc'))
    self.assertEqual(expected_toc, intro_data.get('toc'))

    expected_text = 'you<h2>first</h2><h3>inner</h3><h2>second</h2>'
    self.assertEqual(expected_text, article_data.Render().text)
    self.assertEqual(expected_text, intro_data.Render().text)


if __name__ == '__main__':
  unittest.main()
