#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from server_instance import ServerInstance
from test_data.canned_data import CANNED_TEST_FILE_SYSTEM_DATA
from test_file_system import TestFileSystem
import unittest

class IntroDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._server = ServerInstance.ForTest(
        TestFileSystem(CANNED_TEST_FILE_SYSTEM_DATA))

  def testIntro(self):
    intro_data_source = self._server.intro_data_source_factory.Create()
    data = intro_data_source.get('test')
    self.assertEqual('hi', data['title'])
    # TODO(kalman): test links.
    expected_toc = [{'subheadings': [{'link': '', 'title': u'inner'}],
                     'link': '',
                     'title': u'first'},
                    {'subheadings': [], 'link': '', 'title': u'second'}]
    self.assertEqual(expected_toc, data['apps_toc'])
    self.assertEqual(expected_toc, data['extensions_toc'])
    self.assertEqual('you<h2>first</h2><h3>inner</h3><h2>second</h2>',
                     data['intro'].render().text)

if __name__ == '__main__':
  unittest.main()
