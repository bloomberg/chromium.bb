#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from document_renderer import DocumentRenderer
from server_instance import ServerInstance
from test_file_system import TestFileSystem
from test_data.canned_data import CANNED_TEST_FILE_SYSTEM_DATA


class DocumentRendererUnittest(unittest.TestCase):
  def setUp(self):
    self._renderer = ServerInstance.ForTest(
        TestFileSystem(CANNED_TEST_FILE_SYSTEM_DATA)).document_renderer

  def testNothingToSubstitute(self):
    document = 'hello world'
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual(document, text)
    self.assertEqual(['Expected a title'], warnings)

  def testTitles(self):
    document = '<h1>title</h1> then $(title) then another $(title)'
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(document, text)
    self.assertEqual(['Found unexpected title "title"'], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual('<h1>title</h1> then title then another $(title)', text)
    self.assertEqual([], warnings)

  def testTocs(self):
    document = ('here is a toc $(table_of_contents) '
                'and another $(table_of_contents)')
    expected_document = ('here is a toc <table-of-contents> and another '
                         '$(table_of_contents)')
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(expected_document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual(expected_document, text)
    self.assertEqual(['Expected a title'], warnings)

  def testRefs(self):
    # The references in this and subsequent tests won't actually be resolved
    document = 'A ref $(ref:baz.baz_e1) here, $(ref:foo.foo_t3 ref title) there'
    expected_document = ('A ref <a href=#type-baz_e1>baz.baz_e1</a> '
                         'here, <a href=#type-foo_t3>ref title</a> '
                         'there')
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(expected_document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual(expected_document, text)
    self.assertEqual(['Expected a title'], warnings)

  def testTitleAndToc(self):
    document = '<h1>title</h1> $(title) and $(table_of_contents)'
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual('<h1>title</h1> $(title) and <table-of-contents>', text)
    self.assertEqual(['Found unexpected title "title"'], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual('<h1>title</h1> title and <table-of-contents>', text)
    self.assertEqual([], warnings)

  def testRefInTitle(self):
    document = '<h1>$(ref:baz.baz_e1 title)</h1> A $(title) was here'
    expected_document_no_title = ('<h1><a href=#type-baz_e1>'
                                  'title</a></h1> A $(title) was here')

    expected_document = ('<h1><a href=#type-baz_e1>title</a></h1>'
                         ' A title was here')
    path = 'some/path/to/document.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(expected_document_no_title, text)
    self.assertEqual([('Found unexpected title "title"')], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual(expected_document, text)
    self.assertEqual([], warnings)

  def testInvalidRef(self):
    # There needs to be more than 100 characters between the invalid ref
    # and the next ref
    document = ('An invalid $(ref:foo.foo_t3 a title with some long '
                'text containing a valid reference pointing to '
                '$(ref:baz.baz_e1) here')
    expected_document = ('An invalid $(ref:foo.foo_t3 a title with some long '
                         'text containing a valid reference pointing to <a'
                         ' href=#type-baz_e1>baz.baz_e1</a> here')
    path = 'some/path/to/document_api.html'

    text, warnings = self._renderer.Render(document, path)
    self.assertEqual(expected_document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, path, render_title=True)
    self.assertEqual(expected_document, text)
    self.assertEqual(['Expected a title'], warnings)


if __name__ == '__main__':
  unittest.main()
