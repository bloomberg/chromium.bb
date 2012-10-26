#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from api_data_source import (APIDataSource,
                             _JscModel,
                             _FormatValue,
                             _RemoveNoDocs)
from compiled_file_system import CompiledFileSystem
from docs_server_utils import GetLinkToRefType
from file_system import FileNotFoundError
from in_memory_object_store import InMemoryObjectStore
from local_file_system import LocalFileSystem
import third_party.json_schema_compiler.json_comment_eater as comment_eater
import third_party.json_schema_compiler.model as model

def _MakeLink(href, text):
  return '<a href="%s">%s</a>' % (href, text)

def _GetType(dict_, name):
  for type_ in dict_['types']:
    if type_['name'] == name:
      return type_

class FakeSamplesDataSource:
  def Create(self, request):
    return {}

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def DISABLED_testSimple(self):
    cache_factory = CompiledFileSystem.Factory(
        LocalFileSystem(self._base_path),
        InMemoryObjectStore('fake_branch'))
    data_source_factory = APIDataSource.Factory(cache_factory,
                                                '.',
                                                FakeSamplesDataSource())
    data_source = data_source_factory.Create({})

    # Take the dict out of the list.
    expected = json.loads(self._ReadLocalFile('expected_test_file.json'))
    expected['permissions'] = None
    test1 = data_source['test_file']
    test1.pop('samples')
    self.assertEqual(expected, test1)
    test2 = data_source['testFile']
    test2.pop('samples')
    self.assertEqual(expected, test2)
    test3 = data_source['testFile.html']
    test3.pop('samples')
    self.assertEqual(expected, test3)
    self.assertRaises(FileNotFoundError, data_source.get, 'junk')

  def _LoadJSON(self, filename):
    return json.loads(comment_eater.Nom(self._ReadLocalFile(filename)))[0]

  def _ToDictTest(self, filename):
    expected_json = json.loads(self._ReadLocalFile('expected_' + filename))
    gen = _JscModel(self._LoadJSON(filename))
    self.assertEquals(expected_json, gen.ToDict())

  def testCreateId(self):
    dict_ = _JscModel(self._LoadJSON('test_file.json')).ToDict()
    self.assertEquals('type-TypeA', dict_['types'][0]['id'])
    self.assertEquals('property-TypeA-b',
                      dict_['types'][0]['properties'][0]['id'])
    self.assertEquals('method-get', dict_['functions'][0]['id'])
    self.assertEquals('event-EventA', dict_['events'][0]['id'])

  def testToDict(self):
    self._ToDictTest('test_file.json')

  def testGetLinkToRefType(self):
    link = GetLinkToRefType('truthTeller', 'liar.Tab')
    self.assertEquals('liar.html#type-Tab', link['href'])
    self.assertEquals('liar.Tab', link['text'])
    link = GetLinkToRefType('truthTeller', 'Tab')
    self.assertEquals('#type-Tab', link['href'])
    self.assertEquals('Tab', link['text'])
    link = GetLinkToRefType('nay', 'lies.chrome.bookmarks.Tab')
    self.assertEquals('lies.chrome.bookmarks.html#type-Tab', link['href'])
    self.assertEquals('lies.chrome.bookmarks.Tab', link['text'])

  def testFormatValue(self):
    self.assertEquals('1,234,567', _FormatValue(1234567))
    self.assertEquals('67', _FormatValue(67))
    self.assertEquals('234,567', _FormatValue(234567))

  def testFormatDescription(self):
    dict_ = _JscModel(self._LoadJSON('ref_test.json')).ToDict()
    self.assertEquals(_MakeLink('#type-type2', 'type2'),
                      _GetType(dict_, 'type1')['description'])
    self.assertEquals(
        'A %s, or %s' % (_MakeLink('#type-type3', 'type3'),
                         _MakeLink('#type-type2', 'type2')),
        _GetType(dict_, 'type2')['description'])
    self.assertEquals(
        '%s != %s' % (_MakeLink('other.html#type-type2', 'other.type2'),
                      _MakeLink('#type-type2', 'type2')),
        _GetType(dict_, 'type3')['description'])

  def testRemoveNoDocs(self):
    d = json.loads(self._ReadLocalFile('nodoc_test.json'))
    _RemoveNoDocs(d)
    self.assertEqual(json.loads(self._ReadLocalFile('expected_nodoc.json')), d)

if __name__ == '__main__':
  unittest.main()
