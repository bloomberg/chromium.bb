#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from fake_fetchers import ConfigureFakeFetchers
from handler import Handler
from servlet import Request

class HandlerTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()

  def testCodeGoogleRedirect(self):
    response = Handler(Request('chrome/extensions/storage.html',
                               'http://code.google.com',
                               {})).Get()
    self.assertEqual(302, response.status)
    self.assertEqual('http://developer.chrome.com/extensions/storage.html',
                     response.headers.get('Location'))

  def testDeveloperGoogleRedirect(self):
    response = Handler(Request.ForTest('')).Get()
    self.assertEqual(302, response.status)
    self.assertEqual('http://developer.google.com/chrome',
                     response.headers.get('Location'))

  def testAppRedirect(self):
    response = Handler(Request.ForTest('apps.html')).Get()
    self.assertEqual(302, response.status)
    self.assertEqual('/apps/about_apps.html', response.headers.get('Location'))

if __name__ == '__main__':
  unittest.main()
