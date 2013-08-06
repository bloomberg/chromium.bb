#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from compiled_file_system import CompiledFileSystem
from object_store_creator import ObjectStoreCreator
from redirector import Redirector
from test_file_system import TestFileSystem
from third_party.json_schema_compiler.json_parse import Parse

HOST = 'http://localhost/'

file_system = TestFileSystem({
  'public': {
    'redirects.json': json.dumps({
      '': '/index.html',
      'home': 'index.html',
      'index.html': 'http://something.absolute.com/'
    }),
    'apps': {
      'redirects.json': json.dumps({
        '': '../index.html',
        'index.html': 'about_apps.html'
      })
    },
    'extensions': {
      'redirects.json': json.dumps({
        'manifest': 'manifest.html'
      }),
      'manifest': {
        'redirects.json': json.dumps({
          '': '../manifest.html',
          'more-info': 'http://lmgtfy.com'
        })
      }
    }
  }
})

class RedirectorTest(unittest.TestCase):
  def setUp(self):
    self._redirector = Redirector(
        CompiledFileSystem.Factory(file_system, ObjectStoreCreator.ForTest()),
        file_system,
        'public')

  def testExternalRedirection(self):
    self.assertEqual(
        'http://something.absolute.com/',
        self._redirector.Redirect(HOST, 'index.html'))
    self.assertEqual(
        'http://lmgtfy.com',
        self._redirector.Redirect(HOST, 'extensions/manifest/more-info'))

  def testAbsoluteRedirection(self):
    self.assertEqual(
        '/apps/about_apps.html',
        self._redirector.Redirect(HOST, 'apps/index.html'))
    self.assertEqual(
      '/index.html', self._redirector.Redirect(HOST, ''))
    self.assertEqual(
      '/index.html', self._redirector.Redirect(HOST, 'home'))

  def testRelativeRedirection(self):
    self.assertEqual(
        '/extensions/manifest.html',
        self._redirector.Redirect(HOST, 'extensions/manifest/'))
    self.assertEqual(
        '/extensions/manifest.html',
        self._redirector.Redirect(HOST, 'extensions/manifest'))
    self.assertEqual(
        '/index.html', self._redirector.Redirect(HOST, 'apps/'))

  def testNotFound(self):
    self.assertEqual(
        None, self._redirector.Redirect(HOST, 'not/a/real/path'))
    self.assertEqual(
        None, self._redirector.Redirect(HOST, 'public/apps/okay.html'))

  def testOldHosts(self):
    self.assertEqual(
        'https://developer.chrome.com/',
        self._redirector.Redirect('http://code.google.com', ''))
    self.assertEqual(
        'https://developer.chrome.com/',
        self._redirector.Redirect('https://code.google.com', ''))

  def testCron(self):
    self._redirector.Cron()

    expected_paths = set([
      'public/redirects.json',
      'public/apps/redirects.json',
      'public/extensions/redirects.json',
      'public/extensions/manifest/redirects.json'
    ])

    for path in expected_paths:
      self.assertEqual(
          Parse(file_system.ReadSingle(path)),
          # Access the cache's object store to see what files were hit during
          # the cron run. Returns strings parsed as JSON.
          self._redirector._cache._file_object_store.Get(
              path).Get()._cache_data)

if __name__ == '__main__':
  unittest.main()
