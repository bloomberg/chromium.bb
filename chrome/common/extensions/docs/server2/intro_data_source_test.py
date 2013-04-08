#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from server_instance import ServerInstance
import svn_constants
from test_file_system import TestFileSystem
import unittest

_TEST_DATA = TestFileSystem.MoveTo(svn_constants.INTRO_PATH, {
  'test.html': '<h1>hello</h1>world<h2>first</h2><h3>inner</h3><h2>second</h2>'
})

class IntroDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._server = ServerInstance.CreateForTest(TestFileSystem(_TEST_DATA))

  def testIntro(self):
    intro_data_source = self._server.intro_data_source_factory.Create()
    data = intro_data_source.get('test')
    self.assertEqual('hello', data['title'])
    # TODO(kalman): test links.
    expected_toc = [{'subheadings': [{'link': '', 'title': u'inner'}],
                     'link': '',
                     'title': u'first'},
                    {'subheadings': [], 'link': '', 'title': u'second'}]
    self.assertEqual(expected_toc, data['apps_toc'])
    self.assertEqual(expected_toc, data['extensions_toc'])
    self.assertEqual('world<h2>first</h2><h3>inner</h3><h2>second</h2>',
                     data['intro'].render().text)

if __name__ == '__main__':
  unittest.main()
