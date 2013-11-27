#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from document_renderer import DocumentRenderer


class _FakeTableOfContentsRenderer(object):
  def __init__(self, string):
    self._string = string

  def Render(self, _):
    return self._string, []


class DocumentRendererUnittest(unittest.TestCase):
  def setUp(self):
    self._renderer = DocumentRenderer(_FakeTableOfContentsRenderer('t-o-c'))

  def testNothingToSubstitute(self):
    document = 'hello world'

    text, warnings = self._renderer.Render(document)
    self.assertEqual(document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, render_title=True)
    self.assertEqual(document, text)
    self.assertEqual(['Expected a title'], warnings)

  def testTitles(self):
    document = '<h1>title</h1> then $(title) then another $(title)'

    text, warnings = self._renderer.Render(document)
    self.assertEqual(document, text)
    self.assertEqual(['Found unexpected title "title"'], warnings)

    text, warnings = self._renderer.Render(document, render_title=True)
    self.assertEqual('<h1>title</h1> then title then another $(title)', text)
    self.assertEqual([], warnings)

  def testTocs(self):
    document = ('here is a toc $(table_of_contents) '
                'and another $(table_of_contents)')
    expected_document = 'here is a toc t-o-c and another $(table_of_contents)'

    text, warnings = self._renderer.Render(document)
    self.assertEqual(expected_document, text)
    self.assertEqual([], warnings)

    text, warnings = self._renderer.Render(document, render_title=True)
    self.assertEqual(expected_document, text)
    self.assertEqual(['Expected a title'], warnings)

  def testTitleAndToc(self):
    document = '<h1>title</h1> $(title) and $(table_of_contents)'

    text, warnings = self._renderer.Render(document)
    self.assertEqual('<h1>title</h1> $(title) and t-o-c', text)
    self.assertEqual(['Found unexpected title "title"'], warnings)

    text, warnings = self._renderer.Render(document, render_title=True)
    self.assertEqual('<h1>title</h1> title and t-o-c', text)
    self.assertEqual([], warnings)


if __name__ == '__main__':
  unittest.main()
