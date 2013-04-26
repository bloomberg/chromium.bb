#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os
import sys
import unittest

from api_data_source import (APIDataSource,
                             _JSCModel,
                             _FormatValue,
                             _RemoveNoDocs,
                             _InlineDocs)
from compiled_file_system import CompiledFileSystem
from file_system import FileNotFoundError
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from reference_resolver import ReferenceResolver

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
                                     ObjectStoreCreator.TestFactory()).Create()

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

  # TODO(kalman): re-enable this when we have a rebase option.
  def DISABLED_testToDict(self):
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

  def testInlineDocs(self):
    schema = {
      "namespace": "storage",
      "properties": {
        "key2": {
          "description": "second key",
          "$ref": "Key"
        },
        "key1": {
          "description": "first key",
          "$ref": "Key"
        }
      },
      "types": [
        {
          "inline_doc": True,
          "type": "string",
          "id": "Key",  # Should be inlined into both properties and be removed
                        # from types.
          "description": "This is a key.",  # This description should disappear.
          "marker": True  # This should appear three times in the output.
        },
        {
          "items": {
            "$ref": "Key"
          },
          "type": "array",
          "id": "KeyList",
          "description": "A list of keys"
        }
      ]
    }

    expected_schema = {
      "namespace": "storage",
      "properties": {
        "key2": {
          "marker": True,
          "type": "string",
          "description": "second key"
        },
        "key1": {
          "marker": True,
          "type": "string",
          "description": "first key"
        }
      },
      "types": [
        {
          "items": {
            "marker": True,
            "type": "string"
          },
          "type": "array",
          "id": "KeyList",
          "description": "A list of keys"
        }
      ]
    }

    inlined_schema = copy.deepcopy(schema)
    _InlineDocs(inlined_schema)
    self.assertEqual(expected_schema, inlined_schema)


if __name__ == '__main__':
  unittest.main()
