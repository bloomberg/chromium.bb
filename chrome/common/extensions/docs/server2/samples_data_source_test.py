#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from samples_data_source import SamplesDataSource
from servlet import Request
from test_util import Server2Path


class SamplesDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = Server2Path('test_data', 'samples_data_source')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _FakeGet(self, key):
    return json.loads(self._ReadLocalFile(key))

  def testFilterSamples(self):
    sds = SamplesDataSource({}, {}, '.', Request.ForTest('/'))
    sds.get = self._FakeGet
    self.assertEquals(json.loads(self._ReadLocalFile('expected.json')),
                      sds.FilterSamples('samples.json', 'bobaloo'))

if __name__ == '__main__':
  unittest.main()
