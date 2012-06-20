#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from handlebar_dict_generator import HandlebarDictGenerator
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
        json.loads(comment_eater.Nom(self._ReadLocalFile(filename)))[0])
    self.assertEquals(expected_json, gen.Generate())

  def testGenerate(self):
    self._GenerateTest('test_file.json')

if __name__ == '__main__':
  unittest.main()
