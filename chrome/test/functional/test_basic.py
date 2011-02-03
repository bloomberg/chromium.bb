#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pyauto_functional
import pyauto


class SimpleTest(pyauto.PyUITest):

  def testCanOpenGoogle(self):
    """Navigate to Google."""
    self.NavigateToURL("http://www.google.com")

  def testHTTP(self):
    """Basic test over local http server."""
    url = self.GetHttpURLForDataPath('english_page.html')
    self.NavigateToURL(url)
    self.assertEqual('This page is in English', self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_functional.Main()
