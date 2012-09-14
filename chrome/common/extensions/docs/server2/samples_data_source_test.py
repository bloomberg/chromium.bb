#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from samples_data_source import SamplesDataSource, _MakeAPILink

class SamplesDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0],
                                   'test_data',
                                   'samples_data_source')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _FakeGet(self, key):
    return json.loads(self._ReadLocalFile(key))

  def testFilterSamples(self):
    sds = SamplesDataSource({}, {}, 'fake_path', None)
    sds.get = self._FakeGet
    self.assertEquals(json.loads(self._ReadLocalFile('expected.json')),
                      sds.FilterSamples('samples.json', 'bobaloo'))

  def testMakeAPILink(self):
    api_list = [
      'foo',
      'bar',
      'baz',
      'jim.bob',
      'jim.bif',
      'joe.bob.bif'
    ]
    self.assertEquals('foo.html#type-baz',
                      _MakeAPILink('type', 'chrome.foo.baz', api_list))
    self.assertEquals('jim.bob.html#type-joe',
                      _MakeAPILink('type', 'chrome.jim.bob.joe', api_list))
    self.assertEquals('joe.bob.bif.html#event-lenny',
                      _MakeAPILink('event',
                                   'chrome.joe.bob.bif.lenny',
                                   api_list))
    self.assertEquals('baz.html#floop-lox',
                      _MakeAPILink('floop', 'chrome.baz.lox', api_list))
    self.assertEquals(None, _MakeAPILink('type', 'chrome.jim.foo', api_list))
    self.assertEquals(None, _MakeAPILink('type', 'chrome.joe.bob', api_list))
    self.assertEquals(None, _MakeAPILink('type', 'chrome.barn.foo', api_list))

if __name__ == '__main__':
  unittest.main()
