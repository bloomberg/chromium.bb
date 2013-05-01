#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from api_data_source import APIDataSource
from compiled_file_system import CompiledFileSystem
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from reference_resolver import ReferenceResolver
from template_data_source import TemplateDataSource
from test_util import DisableLogging
from third_party.handlebar import Handlebar
from servlet import Request

class _FakeFactory(object):
  def __init__(self, input_dict=None):
    if input_dict is None:
      self._input_dict = {}
    else:
      self._input_dict = input_dict

  def Create(self, *args, **optargs):
    return self._input_dict

class TemplateDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0],
                                   'test_data',
                                   'template_data_source')
    self._fake_api_list_data_source_factory = _FakeFactory()
    self._fake_intro_data_source_factory = _FakeFactory()
    self._fake_samples_data_source_factory = _FakeFactory()
    self._fake_sidenav_data_source_factory = _FakeFactory()

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _RenderTest(self, name, data_source):
    template_name = name + '_tmpl.html'
    template = Handlebar(self._ReadLocalFile(template_name))
    self.assertEquals(
        self._ReadLocalFile(name + '_expected.html'),
        data_source.Render(template_name))

  def _CreateTemplateDataSource(self, compiled_fs_factory, api_data=None):
    if api_data is None:
      api_data_factory = APIDataSource.Factory(compiled_fs_factory, 'fake_path')
    else:
      api_data_factory = _FakeFactory(api_data)
    reference_resolver_factory = ReferenceResolver.Factory(
        api_data_factory,
        self._fake_api_list_data_source_factory,
        ObjectStoreCreator.TestFactory())
    @DisableLogging('error')  # "was never set" error
    def create_from_factory(factory):
      path = 'extensions/foo'
      return factory.Create(Request(path, {}), path)
    return create_from_factory(TemplateDataSource.Factory(
        'fake_channel',
        api_data_factory,
        self._fake_api_list_data_source_factory,
        self._fake_intro_data_source_factory,
        self._fake_samples_data_source_factory,
        self._fake_sidenav_data_source_factory,
        compiled_fs_factory,
        reference_resolver_factory,
        '.',
        '.'))

  def testSimple(self):
    self._base_path = os.path.join(self._base_path, 'simple')
    fetcher = LocalFileSystem(self._base_path)
    compiled_fs_factory = CompiledFileSystem.Factory(
        fetcher,
        ObjectStoreCreator.TestFactory())
    t_data_source = self._CreateTemplateDataSource(
        compiled_fs_factory,
        ObjectStoreCreator.TestFactory())
    template_a1 = Handlebar(self._ReadLocalFile('test1.html'))
    self.assertEqual(template_a1.render({}, {'templates': {}}).text,
        t_data_source.get('test1').render({}, {'templates': {}}).text)

    template_a2 = Handlebar(self._ReadLocalFile('test2.html'))
    self.assertEqual(template_a2.render({}, {'templates': {}}).text,
        t_data_source.get('test2').render({}, {'templates': {}}).text)

  @DisableLogging('warning')
  def testNotFound(self):
    self._base_path = os.path.join(self._base_path, 'simple')
    fetcher = LocalFileSystem(self._base_path)
    compiled_fs_factory = CompiledFileSystem.Factory(
        fetcher,
        ObjectStoreCreator.TestFactory())
    t_data_source = self._CreateTemplateDataSource(
        compiled_fs_factory,
        ObjectStoreCreator.TestFactory())
    self.assertEqual(None, t_data_source.get('junk.html'))

  def testPartials(self):
    self._base_path = os.path.join(self._base_path, 'partials')
    fetcher = LocalFileSystem(self._base_path)
    compiled_fs_factory = CompiledFileSystem.Factory(
        fetcher,
        ObjectStoreCreator.TestFactory())
    t_data_source = self._CreateTemplateDataSource(compiled_fs_factory)
    self.assertEqual(
        self._ReadLocalFile('test_expected.html'),
        t_data_source.get('test_tmpl').render(
            json.loads(self._ReadLocalFile('input.json')), t_data_source).text)

  def testRender(self):
    self._base_path = os.path.join(self._base_path, 'render')
    fetcher = LocalFileSystem(self._base_path)
    context = json.loads(self._ReadLocalFile('test1.json'))
    compiled_fs_factory = CompiledFileSystem.Factory(
        fetcher,
        ObjectStoreCreator.TestFactory())
    self._RenderTest(
        'test1',
        self._CreateTemplateDataSource(
            compiled_fs_factory,
            api_data=json.loads(self._ReadLocalFile('test1.json'))))
    self._RenderTest(
        'test2',
        self._CreateTemplateDataSource(
            compiled_fs_factory,
            api_data=json.loads(self._ReadLocalFile('test2.json'))))

if __name__ == '__main__':
  unittest.main()
