#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from fetcher_cache import FetcherCache
from local_fetcher import LocalFetcher
from template_data_source import TemplateDataSource
from third_party.handlebar import Handlebar

class TemplateDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'template_data_source')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _RenderTest(self, name, data_source):
    template_name = name + '_tmpl.html'
    template = Handlebar(self._ReadLocalFile(template_name))
    context = json.loads(self._ReadLocalFile(name + '.json'))
    self.assertEquals(
        self._ReadLocalFile(name + '_expected.html'),
        data_source.Render(template_name, context))

  def testSimple(self):
    self._base_path = os.path.join(self._base_path, 'simple')
    fetcher = LocalFetcher(self._base_path)
    cache_builder = FetcherCache.Builder(fetcher, 0)
    t_data_source = TemplateDataSource(cache_builder, ['./'])

    template_a1 = Handlebar(self._ReadLocalFile('test1.html'))
    self.assertEqual(template_a1.render({}, {'templates': {}}).text,
        t_data_source['test1'].render({}, {'templates': {}}).text)

    template_a2 = Handlebar(self._ReadLocalFile('test2.html'))
    self.assertEqual(template_a2.render({}, {'templates': {}}).text,
        t_data_source['test2'].render({}, {'templates': {}}).text)

    self.assertEqual(None, t_data_source['junk.html'])

  def testPartials(self):
    self._base_path = os.path.join(self._base_path, 'partials')
    fetcher = LocalFetcher(self._base_path)
    cache_builder = FetcherCache.Builder(fetcher, 0)
    t_data_source = TemplateDataSource(cache_builder, ['./'])

    self.assertEqual(self._ReadLocalFile('test.html'),
        t_data_source['test_tmpl'].render(
            json.loads(self._ReadLocalFile('input.json')), t_data_source).text)

  def testRender(self):
    self._base_path = os.path.join(self._base_path, 'render')
    fetcher = LocalFetcher(self._base_path)
    cache_builder = FetcherCache.Builder(fetcher, 0)
    t_data_source = TemplateDataSource(cache_builder, ['./'])
    self._RenderTest('test1', t_data_source)
    self._RenderTest('test2', t_data_source)



if __name__ == '__main__':
  unittest.main()
