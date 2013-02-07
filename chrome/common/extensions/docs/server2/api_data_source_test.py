#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from api_data_source import (APIDataSource,
                             _JSCModel,
                             _FormatValue,
                             _RemoveNoDocs)
from compiled_file_system import CompiledFileSystem
from file_system import FileNotFoundError
from in_memory_object_store import InMemoryObjectStore
from local_file_system import LocalFileSystem
from reference_resolver import ReferenceResolver
import third_party.json_schema_compiler.model as model

def _MakeLink(href, text):
  return '<a href="%s">%s</a>' % (href, text)

def _GetType(dict_, name):
  for type_ in dict_['types']:
    if type_['name'] == name:
      return type_

class FakeSamplesDataSource(object):
  def Create(self, request):
    return {}

class FakeAPIAndListDataSource(object):
  def __init__(self, json_data):
    self._json = json_data

  def Create(self, *args, **kwargs):
    return self

  def get(self, key):
    if key not in self._json:
      raise FileNotFoundError(key)
    return self._json[key]

  def GetAllNames(self):
    return self._json.keys()

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _CreateRefResolver(self, filename):
    data_source = FakeAPIAndListDataSource(
        self._LoadJSON(filename))
    return ReferenceResolver.Factory(data_source,
                                     data_source,
                                     InMemoryObjectStore('')).Create()

  def DISABLED_testSimple(self):
    cache_factory = CompiledFileSystem.Factory(
        LocalFileSystem(self._base_path),
        InMemoryObjectStore('fake_branch'))
    data_source_factory = APIDataSource.Factory(cache_factory,
                                                '.')
    data_source_factory.SetSamplesDataSourceFactory(FakeSamplesDataSource())
    data_source = data_source_factory.Create({}, disable_refs=True)

    # Take the dict out of the list.
    expected = json.loads(self._ReadLocalFile('expected_test_file.json'))
    expected['permissions'] = None
    test1 = data_source.get('test_file')
    test1.pop('samples')
    self.assertEqual(expected, test1)
    test2 = data_source.get('testFile')
    test2.pop('samples')
    self.assertEqual(expected, test2)
    test3 = data_source.get('testFile.html')
    test3.pop('samples')
    self.assertEqual(expected, test3)
    self.assertRaises(FileNotFoundError, data_source.get, 'junk')

  def _LoadJSON(self, filename):
    return json.loads(self._ReadLocalFile(filename))

  def testCreateId(self):
    data_source = FakeAPIAndListDataSource(
        self._LoadJSON('test_file_data_source.json'))
    dict_ = _JSCModel(self._LoadJSON('test_file.json')[0],
                      self._CreateRefResolver('test_file_data_source.json'),
                      False).ToDict()
    self.assertEquals('type-TypeA', dict_['types'][0]['id'])
    self.assertEquals('property-TypeA-b',
                      dict_['types'][0]['properties'][0]['id'])
    self.assertEquals('method-get', dict_['functions'][0]['id'])
    self.assertEquals('event-EventA', dict_['events'][0]['id'])

  def testToDict(self):
    filename = 'test_file.json'
    expected_json = self._LoadJSON('expected_' + filename)
    data_source = FakeAPIAndListDataSource(
        self._LoadJSON('test_file_data_source.json'))
    dict_ = _JSCModel(self._LoadJSON(filename)[0],
                      self._CreateRefResolver('test_file_data_source.json'),
                      False).ToDict()
    self.assertEquals(expected_json, dict_)

  def testFormatValue(self):
    self.assertEquals('1,234,567', _FormatValue(1234567))
    self.assertEquals('67', _FormatValue(67))
    self.assertEquals('234,567', _FormatValue(234567))

  def testFormatDescription(self):
    dict_ = _JSCModel(self._LoadJSON('ref_test.json')[0],
                      self._CreateRefResolver('ref_test_data_source.json'),
                      False).ToDict()
    self.assertEquals(_MakeLink('ref_test.html#type-type2', 'type2'),
                      _GetType(dict_, 'type1')['description'])
    self.assertEquals(
        'A %s, or %s' % (_MakeLink('ref_test.html#type-type3', 'type3'),
                         _MakeLink('ref_test.html#type-type2', 'type2')),
        _GetType(dict_, 'type2')['description'])
    self.assertEquals(
        '%s != %s' % (_MakeLink('other.html#type-type2', 'other.type2'),
                      _MakeLink('ref_test.html#type-type2', 'type2')),
        _GetType(dict_, 'type3')['description'])

  def testRemoveNoDocs(self):
    d = self._LoadJSON('nodoc_test.json')
    _RemoveNoDocs(d)
    self.assertEqual(self._LoadJSON('expected_nodoc.json'), d)

if __name__ == '__main__':
  unittest.main()
