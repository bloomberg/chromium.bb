#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from api_list_data_source import APIListDataSource
from server_instance import ServerInstance
from test_file_system import TestFileSystem


def _ToTestData(obj):
  '''Transforms |obj| into test data by turning a list of files into an object
  mapping that file to its contents (derived from its name).
  '''
  return dict((name, name) for name in obj)


def _ToTestFeatures(names):
  '''Transforms a list of strings into a minimal JSON features object.
  '''
  return dict((name, {'name': name, 'platforms': platforms})
              for name, platforms in names)


_TEST_API_FEATURES = _ToTestFeatures([
  ('alarms', ['apps', 'extensions']),
  ('app.window', ['apps']),
  ('browserAction', ['extensions']),
  ('experimental.bluetooth', ['apps']),
  ('experimental.history', ['extensions']),
  ('experimental.power', ['apps', 'extensions']),
  ('infobars', ['extensions']),
  ('something_internal', ['apps']),
  ('something_else_internal', ['extensions']),
  ('storage', ['apps', 'extensions'])
])


_TEST_DATA = {
  'api': {
    '_api_features.json': json.dumps(_TEST_API_FEATURES),
    '_manifest_features.json': '{}',
    '_permission_features.json': '{}',
  },
  'docs': {
    'templates': {
      'json': {
        'manifest.json': '{}',
        'permissions.json': '{}',
      },
      'public': {
        'apps': _ToTestData([
          'alarms.html',
          'app_window.html',
          'experimental_bluetooth.html',
          'experimental_power.html',
          'storage.html',
        ]),
        'extensions': _ToTestData([
          'alarms.html',
          'browserAction.html',
          'experimental_history.html',
          'experimental_power.html',
          'infobars.html',
          'storage.html',
        ]),
      },
    },
  },
}


class APIListDataSourceTest(unittest.TestCase):
  def setUp(self):
    server_instance = ServerInstance.ForTest(TestFileSystem(_TEST_DATA))
    self._factory = APIListDataSource.Factory(
        server_instance.compiled_fs_factory,
        server_instance.host_file_system_provider.GetTrunk(),
        server_instance.features_bundle,
        server_instance.object_store_creator)

  def testApps(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'alarms',
          'platforms': ['apps', 'extensions']
        },
        {
          'name': 'app.window',
          'platforms': ['apps']
        },
        {
          'name': 'storage',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        api_list.get('apps').get('chrome'))

  def testExperimentalApps(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.bluetooth',
          'platforms': ['apps']
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('apps').get('experimental')))

  def testExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'alarms',
          'platforms': ['apps', 'extensions']
        },
        {
          'name': 'browserAction',
          'platforms': ['extensions']
        },
        {
          'name': 'infobars',
          'platforms': ['extensions']
        },
        {
          'name': 'storage',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('extensions').get('chrome')))

  def testExperimentalExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.history',
          'platforms': ['extensions']
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('extensions').get('experimental')))

if __name__ == '__main__':
  unittest.main()
