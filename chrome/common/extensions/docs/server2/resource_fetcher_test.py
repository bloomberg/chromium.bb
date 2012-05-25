#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import test_urlfetch

from resource_fetcher import SubversionFetcher

class TestSubversionFetcher(SubversionFetcher):
  def _GetURLFromBranch(self, branch):
    if branch == 'trunk':
      return 'subversion_fetcher/trunk/'
    return 'subversion_fetcher/' + branch + '/'

class SubversionFetcherTest(unittest.TestCase):
  def testFetchResource(self):
    s_fetcher = TestSubversionFetcher(test_urlfetch)
    self.assertEquals('trunk test\n',
        s_fetcher.FetchResource('trunk', 'test.json').content)
    self.assertEquals('branch1 test\n',
        s_fetcher.FetchResource('branch1', 'test.json').content)
    self.assertEquals('branch2 test\n',
        s_fetcher.FetchResource('branch2', 'test.json').content)

if __name__ == '__main__':
  unittest.main()
