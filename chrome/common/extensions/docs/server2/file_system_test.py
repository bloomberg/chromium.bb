#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_file_system import TestFileSystem

file_system = TestFileSystem({
  'templates': {
    'public': {
      'apps': {
        '404.html': '',
        'a11y.html': ''
      },
      'extensions': {
        '404.html': '',
        'cookies.html': ''
      },
      'redirects.json': 'redirect'
    },
    'json': {
      'manifest.json': 'manifest'
    }
  }
})

class FileSystemTest(unittest.TestCase):
  def testWalk(self):
    expected_files = set([
      'templates/public/apps/404.html',
      'templates/public/apps/a11y.html',
      'templates/public/extensions/404.html',
      'templates/public/extensions/cookies.html',
      'templates/public/redirects.json',
      'templates/json/manifest.json'
    ])

    expected_dirs = set([
      '/templates/',
      'templates/public/',
      'templates/public/apps/',
      'templates/public/extensions/',
      'templates/json/'
    ])

    all_files = set()
    all_dirs = set()
    for root, dirs, files in file_system.Walk(''):
      all_files.update(root + '/' + name for name in files)
      all_dirs.update(root + '/' + name for name in dirs)

    self.assertEqual(expected_files, all_files)
    self.assertEqual(expected_dirs, all_dirs)

  def testSubWalk(self):
    expected_files = set([
      '/redirects.json',
      'apps/404.html',
      'apps/a11y.html',
      'extensions/404.html',
      'extensions/cookies.html'
    ])

    all_files = set()
    for root, dirs, files in file_system.Walk('templates/public'):
      all_files.update(root + '/' + name for name in files)

    self.assertEqual(expected_files, all_files)

if __name__ == '__main__':
  unittest.main()
