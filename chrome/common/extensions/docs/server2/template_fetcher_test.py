#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import test_urlfetch

from local_fetcher import LocalFetcher
from template_fetcher import TemplateFetcher
from third_party.handlebar import Handlebar

def _ReadFile(filename):
  with open(filename, 'r') as f:
    return f.read()

class TestFetcher(LocalFetcher):
  def FetchResource(self, branch, path):
    result = self._Resource()
    result.content = _ReadFile(os.path.join(self.base_path, branch, path))
    return result

class TemplateFetcherTest(unittest.TestCase):
  def testFetcher(self):
    path = os.path.join('test_data', 'template_fetcher')
    fetcher = TestFetcher(path)
    t_fetcher_a = TemplateFetcher('a', fetcher)
    t_fetcher_b = TemplateFetcher('b', fetcher)

    template_a1 = Handlebar(_ReadFile(os.path.join(path, 'a', 'test1.html')))
    self.assertEqual(template_a1.render('{}', {'templates': {}}).text,
        t_fetcher_a['test1.html'].render('{}', {'templates': {}}).text)

    template_a2 = Handlebar(_ReadFile(os.path.join(path, 'a', 'test2.html')))
    self.assertEqual(template_a2.render('{}', {'templates': {}}).text,
        t_fetcher_a['test2.html'].render('{}', {'templates': {}}).text)

    template_b1 = Handlebar(_ReadFile(os.path.join(path, 'b', 'test1.html')))
    self.assertEqual(template_b1.render('{}', {'templates': {}}).text,
        t_fetcher_b['test1.html'].render('{}', {'templates': {}}).text)

    self.assertEqual('', t_fetcher_b['junk.html'])

if __name__ == '__main__':
  unittest.main()
