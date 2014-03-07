#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import json

from api_list_data_source import APIListDataSource
from extensions_paths import EXTENSIONS
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
  def platforms_to_extension_types(platforms):
    return ['platform_app' if platform == 'apps' else 'extension'
            for platform in platforms]
  features = dict((name, {
                     'name': name,
                     'extension_types': platforms_to_extension_types(platforms),
                   }) for name, platforms in names)
  features['sockets.udp']['channel'] = 'dev'
  return features


def _ToTestApiData(names):
  api_data = dict((name, [{'namespace': name, 'description': description}])
              for name, description in names)
  return api_data


def _ToTestApiSchema(names, apis):
  for name, json_file in names:
    apis['api'][json_file] = json.dumps(_TEST_API_DATA[name])
  return apis


_TEST_API_FEATURES = _ToTestFeatures([
  ('alarms', ['apps', 'extensions']),
  ('app.window', ['apps']),
  ('browserAction', ['extensions']),
  ('experimental.bluetooth', ['apps']),
  ('experimental.history', ['extensions'],),
  ('experimental.power', ['apps', 'extensions']),
  ('infobars', ['extensions']),
  ('something_internal', ['apps']),
  ('something_else_internal', ['extensions']),
  ('storage', ['apps', 'extensions']),
  ('sockets.udp', ['apps', 'extensions'])
])


_TEST_API_DATA = _ToTestApiData([
  ('alarms', u'<code>alarms</code>'),
  ('app.window', u'<code>app.window</code>'),
  ('browserAction', u'<code>browserAction</code>'),
  ('experimental.bluetooth', u'<code>experimental.bluetooth</code>'),
  ('experimental.history', u'<code>experimental.history</code>'),
  ('experimental.power', u'<code>experimental.power</code>'),
  ('infobars', u'<code>infobars</code>'),
  ('something_internal', u'<code>something_internal</code>'),
  ('something_else_internal', u'<code>something_else_internal</code>'),
  ('storage', u'<code>storage</code>'),
  ('sockets.udp', u'<code>sockets.udp</code>')
])


_TEST_API_SCHEMA = [
  ('alarms', 'alarms.json'),
  ('app.window', 'app_window.json'),
  ('browserAction', 'browser_action.json'),
  ('experimental.bluetooth', 'experimental_bluetooth.json'),
  ('experimental.history', 'experimental_history.json'),
  ('experimental.power', 'experimental_power.json'),
  ('infobars', 'infobars.json'),
  ('something_internal', 'something_internal.json'),
  ('something_else_internal', 'something_else_internal.json'),
  ('storage', 'storage.json'),
  ('sockets.udp', 'sockets_udp.json')
]


_TEST_DATA = _ToTestApiSchema(_TEST_API_SCHEMA, {
  'api': {
    '_api_features.json': json.dumps(_TEST_API_FEATURES),
    '_manifest_features.json': '{}',
    '_permission_features.json': '{}',
  },
  'docs': {
    'templates': {
      'json': {
        'api_availabilities.json': '{}',
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
          'sockets_udp.html'
        ]),
        'extensions': _ToTestData([
          'alarms.html',
          'browserAction.html',
          'experimental_history.html',
          'experimental_power.html',
          'infobars.html',
          'storage.html',
          'sockets_udp.html'
        ]),
      },
    },
  },
})


class APIListDataSourceTest(unittest.TestCase):
  def setUp(self):
    server_instance = ServerInstance.ForTest(
        TestFileSystem(_TEST_DATA, relative_to=EXTENSIONS))
    self._factory = APIListDataSource.Factory(
        server_instance.compiled_fs_factory,
        server_instance.host_file_system_provider.GetTrunk(),
        server_instance.features_bundle,
        server_instance.object_store_creator,
        server_instance.api_models,
        server_instance.availability_finder,
        server_instance.api_categorizer)
    self.maxDiff = None

  def testApps(self):
    api_list = self._factory.Create()
    self.assertEqual({
        'stable': [
          {
            'name': 'alarms',
            'platforms': ['apps', 'extensions'],
            'version': 5,
            'description': u'<code>alarms</code>'
          },
          {
            'name': 'app.window',
            'platforms': ['apps'],
            # Availability logic will look for a camelCase format filename
            # (i.e. 'app.window.html') at version 20 and below, but the
            # unix_name format above won't be found at these versions.
            'version': 21,
            'description': u'<code>app.window</code>'
          },
          {
            'name': 'storage',
            'platforms': ['apps', 'extensions'],
            'last': True,
            'version': 5,
            'description': u'<code>storage</code>'
          }],
        'dev': [
          {
            'name': 'sockets.udp',
            'platforms': ['apps', 'extensions'],
            'last': True,
            'description': u'<code>sockets.udp</code>'
          }],
        'beta': [],
        'trunk': []
        }, api_list.get('apps').get('chrome'))

  def testExperimentalApps(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.bluetooth',
          'platforms': ['apps'],
          'description': u'<code>experimental.bluetooth</code>'
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'last': True,
          'description': u'<code>experimental.power</code>'
        }], api_list.get('apps').get('experimental'))

  def testExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual({
        'stable': [
          {
            'name': 'alarms',
            'platforms': ['apps', 'extensions'],
            'version': 5,
            'description': u'<code>alarms</code>'
          },
          {
            'name': 'browserAction',
            'platforms': ['extensions'],
            # See comment above for 'app.window'.
            'version': 21,
            'description': u'<code>browserAction</code>'
          },
          {
            'name': 'infobars',
            'platforms': ['extensions'],
            'version': 5,
            'description': u'<code>infobars</code>'
          },
          {
            'name': 'storage',
            'platforms': ['apps', 'extensions'],
            'last': True,
            'version': 5,
            'description': u'<code>storage</code>'
          }],
        'dev': [
          {
            'name': 'sockets.udp',
            'platforms': ['apps', 'extensions'],
            'last': True,
            'description': u'<code>sockets.udp</code>'
          }],
        'beta': [],
        'trunk': []
        }, api_list.get('extensions').get('chrome'))

  def testExperimentalExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.history',
          'platforms': ['extensions'],
          'description': u'<code>experimental.history</code>'
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'description': u'<code>experimental.power</code>',
          'last': True
        }], api_list.get('extensions').get('experimental'))

if __name__ == '__main__':
  unittest.main()
