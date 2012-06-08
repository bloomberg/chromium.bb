#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from local_fetcher import LocalFetcher

class LocalFetcherTest(unittest.TestCase):
  def testFetchResource(self):
    fetcher = LocalFetcher('')
    local_path = os.path.join('test_data', 'local_fetcher')
    self.assertEquals('test1\n', fetcher.FetchResource(
        os.path.join(local_path, 'test1.txt')).content)
    self.assertEquals('test2\n', fetcher.FetchResource(
        os.path.join(local_path, 'test2.txt')).content)
    self.assertEquals('test3\n', fetcher.FetchResource(
        os.path.join(local_path, 'test3.txt')).content)

if __name__ == '__main__':
  unittest.main()
