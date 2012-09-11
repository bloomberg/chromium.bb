#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from docs_server_utils import GetLinkToRefType
from handlebar_dict_generator import HandlebarDictGenerator
from handlebar_dict_generator import _FormatValue
import third_party.json_schema_compiler.json_comment_eater as comment_eater
import third_party.json_schema_compiler.model as model

def _MakeLink(href, text):
  return '<a href="%s">%s</a>' % (href, text)

def _GetType(dict_, name):
  for type_ in dict_['types']:
    if type_['name'] == name:
      return type_

class DictGeneratorTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _LoadJSON(self, filename):
    return json.loads(comment_eater.Nom(self._ReadLocalFile(filename)))[0]

  def _GenerateTest(self, filename):
    expected_json = json.loads(self._ReadLocalFile('expected_' + filename))
    gen = HandlebarDictGenerator(self._LoadJSON(filename))
    self.assertEquals(expected_json, gen.Generate())

  def testGenerate(self):
    self._GenerateTest('test_file.json')

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
    dict_ = HandlebarDictGenerator(self._LoadJSON('ref_test.json')).Generate()
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

if __name__ == '__main__':
  unittest.main()
