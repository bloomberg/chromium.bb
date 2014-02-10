#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from extensions_paths import SERVER2
from server_instance import ServerInstance
from template_data_source import TemplateDataSource
from test_util import DisableLogging, ReadFile, Server2Path
from third_party.handlebar import Handlebar


class TemplateDataSourceTest(unittest.TestCase):

  def _ReadFile(self, *path):
    return ReadFile(SERVER2, 'test_data', 'template_data_source', *path)

  def _CreateTemplateDataSource(self, partial_dir):
    return TemplateDataSource(
        ServerInstance.ForLocal(),
        None,  # Request
        partial_dir='%stest_data/template_data_source/%s/' %
                    (SERVER2, partial_dir))

  def testSimple(self):
    template_data_source = self._CreateTemplateDataSource('simple')
    template_a1 = Handlebar(self._ReadFile('simple', 'test1.html'))
    context = [{}, {'templates': {}}]
    self.assertEqual(
        template_a1.Render(*context).text,
        template_data_source.get('test1').Render(*context).text)
    template_a2 = Handlebar(self._ReadFile('simple', 'test2.html'))
    self.assertEqual(
        template_a2.Render(*context).text,
        template_data_source.get('test2').Render(*context).text)

  @DisableLogging('warning')
  def testNotFound(self):
    template_data_source = self._CreateTemplateDataSource('simple')
    self.assertEqual(None, template_data_source.get('junk'))

  @DisableLogging('warning')
  def testPartials(self):
    template_data_source = self._CreateTemplateDataSource('partials')
    context = json.loads(self._ReadFile('partials', 'input.json'))
    self.assertEqual(
        self._ReadFile('partials', 'test_expected.html'),
        template_data_source.get('test_tmpl').Render(
            context, template_data_source).text)


if __name__ == '__main__':
  unittest.main()
