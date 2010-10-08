#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import pyauto_functional
import pyauto


class FindMatchTests(pyauto.PyUITest):

  def testCanFindMatchCount(self):
    """Verify Find match count for valid search"""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    self.NavigateToURL(url)
    self.assertEqual(1, self.FindInPage('title')['match_count'])

  def testCanFindMatchCountFail(self):
    """Verify Find match count for invalid search"""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    self.NavigateToURL(url)
    self.assertEqual(0, self.FindInPage('blah')['match_count'])


if __name__ == '__main__':
  pyauto_functional.Main()
