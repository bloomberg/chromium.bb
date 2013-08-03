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

redirector = Redirector(
    CompiledFileSystem.Factory(file_system, ObjectStoreCreator.ForTest()),
    None,
    'public')

class RedirectorTest(unittest.TestCase):
  def testExternalRedirection(self):
    self.assertEqual(
        'http://something.absolute.com/',
        redirector.Redirect(HOST, 'index.html'))
    self.assertEqual(
        'http://lmgtfy.com',
        redirector.Redirect(HOST, 'extensions/manifest/more-info'))

  def testAbsoluteRedirection(self):
    self.assertEqual(
        '/apps/about_apps.html', redirector.Redirect(HOST, 'apps/index.html'))
    self.assertEqual(
      '/index.html', redirector.Redirect(HOST, ''))
    self.assertEqual(
      '/index.html', redirector.Redirect(HOST, 'home'))

  def testRelativeRedirection(self):
    self.assertEqual(
        '/extensions/manifest.html',
        redirector.Redirect(HOST, 'extensions/manifest/'))
    self.assertEqual(
        '/extensions/manifest.html',
        redirector.Redirect(HOST, 'extensions/manifest'))
    self.assertEqual(
        '/index.html', redirector.Redirect(HOST, 'apps/'))

  def testNotFound(self):
    self.assertEqual(None, redirector.Redirect(HOST, 'not/a/real/path'))
    self.assertEqual(None, redirector.Redirect(HOST, 'public/apps/okay.html'))

  def testOldHosts(self):
    self.assertEqual(
        'https://developer.chrome.com/',
        redirector.Redirect('http://code.google.com', ''))
    self.assertEqual(
        'https://developer.chrome.com/',
        redirector.Redirect('https://code.google.com', ''))

if __name__ == '__main__':
  unittest.main()
