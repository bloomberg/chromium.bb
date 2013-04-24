#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from api_list_data_source import APIListDataSource
from compiled_file_system import CompiledFileSystem
from copy import deepcopy
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem

def _ToTestData(obj):
  '''Transforms |obj| into test data by turning a list of files into an object
  mapping that file to its contents (derived from its name).
  '''
  return (dict((name, name) for name in obj) if isinstance(obj, list) else
          dict((key, _ToTestData(value)) for key, value in obj.items()))

_TEST_DATA = _ToTestData({
  'api': [
    'alarms.idl',
    'app_window.idl',
    'browser_action.json',
    'experimental_bluetooth.idl',
    'experimental_history.idl',
    'experimental_infobars.idl',
    'experimental_power.idl',
    'something_internal.idl',
    'something_else_internal.json',
    'storage.json',
  ],
  'public': {
    'apps': [
      'alarms.html',
      'app_window.html',
      'experimental_bluetooth.html',
      'experimental_power.html',
      'storage.html',
    ],
    'extensions': [
      'alarms.html',
      'browserAction.html',
      'experimental_history.html',
      'experimental_infobars.html',
      'experimental_power.html',
      'storage.html',
    ],
  },
})

class APIListDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._factory = APIListDataSource.Factory(
        CompiledFileSystem.Factory(TestFileSystem(deepcopy(_TEST_DATA)),
                                   ObjectStoreCreator.TestFactory()),
        'api',
        'public')

  def testApps(self):
    api_list = self._factory.Create()
    self.assertEqual([{'name': 'alarms'},
                      {'name': 'app.window'},
                      {'name': 'storage', 'last': True}],
                      api_list.get('apps').get('chrome'))

  def testExperimentalApps(self):
    api_list = self._factory.Create()
    self.assertEqual([{'name': 'experimental.bluetooth'},
                      {'name': 'experimental.power', 'last': True}],
                     sorted(api_list.get('apps').get('experimental')))

  def testExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([{'name': 'alarms'},
                      {'name': 'browserAction'},
                      {'name': 'storage', 'last': True}],
                     sorted(api_list.get('extensions').get('chrome')))

  def testExperimentalApps(self):
    api_list = self._factory.Create()
    self.assertEqual([{'name': 'experimental.history'},
                      {'name': 'experimental.infobars'},
                      {'name': 'experimental.power', 'last': True}],
                     sorted(api_list.get('extensions').get('experimental')))

if __name__ == '__main__':
  unittest.main()
