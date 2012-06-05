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
    fetcher = TestSubversionFetcher('', test_urlfetch)
    self.assertEquals('trunk test\n',
        fetcher.FetchResource('trunk', '/test.txt').content)
    self.assertEquals('branch1 test\n',
        fetcher.FetchResource('branch1', '/test.txt').content)
    self.assertEquals('branch2 test\n',
        fetcher.FetchResource('branch2', '/test.txt').content)

if __name__ == '__main__':
  unittest.main()
