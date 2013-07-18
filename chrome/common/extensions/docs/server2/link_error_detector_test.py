#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from link_error_detector import LinkErrorDetector
from servlet import Response
from test_file_system import TestFileSystem

file_system = TestFileSystem({
  'docs': {
    'templates': {
      'public': {
        'apps': {
          '404.html': '404',
          'index.html': '''
            <h1 id="actual-top">Hello</h1>
            <a href="#top">world</p>
            <a href="#actual-top">!</a>
            <a href="broken.json"></a>
            <a href="crx.html"></a>
            ''',
          'crx.html': '''
            <a href="index.html#actual-top">back</a>
            <a href="broken.html"></a>
            <a href="devtools.events.html">do to underscore translation</a>
            ''',
          'devtools_events.html': '''
            <a href=" http://www.google.com/">leading space in href</a>
            <a href=" index.html">home</a>
            <a href="index.html#invalid"></a>
            <a href="fake.html#invalid"></a>
            ''',
          'unreachable.html': '''
            <p>so lonely</p>
            <a href="#aoesu"></a>
            <a href="invalid.html"></a>
            ''',
          'devtools_disconnected.html': ''
        }
      }
    },
    'static': {},
    'examples': {},
  }
})

class LinkErrorDetectorTest(unittest.TestCase):
  def render(self, path):
    try:
      return Response(
          content=file_system.ReadSingle('docs/templates/public/' + path),
          status=200)
    except AttributeError:
      return Response(status=404)

  def testGetBrokenLinks(self):
    expected_broken_links = set([
      ('apps/crx.html', 'apps/broken.html'),
      ('apps/index.html', 'apps/broken.json'),
      ('apps/unreachable.html', 'apps/invalid.html'),
      ('apps/devtools_events.html', 'apps/fake.html#invalid')])

    expected_broken_anchors  = set([
      ('apps/devtools_events.html', 'apps/index.html#invalid'),
      ('apps/unreachable.html', '#aoesu')])

    link_error_detector = LinkErrorDetector(
        file_system, self.render, 'templates/public', ('apps/index.html'))
    broken_links, broken_anchors = link_error_detector.GetBrokenLinks()

    self.assertEqual(expected_broken_links, set(broken_links))
    self.assertEqual(expected_broken_anchors, set(broken_anchors))

  def testGetOrphanedPages(self):
    expected_orphaned_pages = set([
      'apps/unreachable.html',
      'apps/devtools_disconnected.html'])

    link_error_detector = LinkErrorDetector(
        file_system, self.render, 'templates/public', ('apps/crx.html',))
    orphaned_pages = link_error_detector.GetOrphanedPages()

    self.assertEqual(expected_orphaned_pages, set(orphaned_pages))

if __name__ == '__main__':
  unittest.main()
