#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import os
import unittest

from handlebar_dict_generator import HandlebarDictGenerator
from handlebar_dict_generator import _GetLinkToRefType
import third_party.json_schema_compiler.json_comment_eater as comment_eater
import third_party.json_schema_compiler.model as model

class DictGeneratorTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _GenerateTest(self, filename):
    expected_json = json.loads(self._ReadLocalFile('expected_' + filename))
    gen = HandlebarDictGenerator(
        json.loads(comment_eater.Nom(self._ReadLocalFile(filename)),
            object_pairs_hook=collections.OrderedDict)[0])
    self.assertEquals(expected_json, gen.Generate())

  def testGenerate(self):
    self._GenerateTest('test_file.json')

  def testGetLinkToRefType(self):
    link = _GetLinkToRefType('truthTeller', 'liar.Tab')
    self.assertEquals(link['href'], 'liar.html#type-Tab')
    self.assertEquals(link['text'], 'Tab')
    link = _GetLinkToRefType('truthTeller', 'Tab')
    self.assertEquals(link['href'], 'truthTeller.html#type-Tab')
    self.assertEquals(link['text'], 'Tab')
    link = _GetLinkToRefType('nay', 'lies.chrome.bookmarks.Tab')
    self.assertEquals(link['href'], 'lies.html#type-chrome.bookmarks.Tab')
    self.assertEquals(link['text'], 'chrome.bookmarks.Tab')

if __name__ == '__main__':
  unittest.main()
