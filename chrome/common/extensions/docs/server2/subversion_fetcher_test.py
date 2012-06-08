#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import test_urlfetch

from subversion_fetcher import SubversionFetcher

class TestSubversionFetcher(SubversionFetcher):
  def _GetURLFromBranch(self, branch):
    if branch == 'trunk':
      return os.path.join('subversion_fetcher', 'trunk')
    return os.path.join('subversion_fetcher', branch)


class SubversionFetcherTest(unittest.TestCase):
  def testFetchResource(self):
    fetcher_t = TestSubversionFetcher('trunk', '', test_urlfetch)
    fetcher_b1 = TestSubversionFetcher('branch1', '', test_urlfetch)
    fetcher_b2 = TestSubversionFetcher('branch2', '', test_urlfetch)
    self.assertEquals('trunk test\n',
        fetcher_t.FetchResource('/test.txt').content)
    self.assertEquals('branch1 test\n',
        fetcher_b1.FetchResource('/test.txt').content)
    self.assertEquals('branch2 test\n',
        fetcher_b2.FetchResource('/test.txt').content)

if __name__ == '__main__':
  unittest.main()
