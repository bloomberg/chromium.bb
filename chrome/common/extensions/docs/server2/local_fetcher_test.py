#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from local_fetcher import LocalFetcher

class LocalFetcherTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'local_fetcher')
    self._fetcher = LocalFetcher(self._base_path)

  def testFetchResource(self):
    self.assertEquals('test1\n',
                      self._fetcher.FetchResource('test1.txt').content)
    self.assertEquals('test2\n',
                      self._fetcher.FetchResource('test2.txt').content)
    self.assertEquals('test3\n',
                      self._fetcher.FetchResource('test3.txt').content)

  def testListDirectory(self):
    expected = ['file%d.html' % i for i in range(7)]
    self.assertEquals(
        expected,
        sorted(self._fetcher.ListDirectory('recursive_list/list').content))
    expected2 = ['recursive_list/' + x for x in expected]
    expected2.extend(['recursive_list/list/' + x for x in expected])
    self.assertEquals(
        expected2,
        sorted(self._fetcher.ListDirectory('recursive_list',
                                    recursive=True).content))

if __name__ == '__main__':
  unittest.main()
