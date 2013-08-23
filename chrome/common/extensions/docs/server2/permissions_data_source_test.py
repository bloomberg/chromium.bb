#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from compiled_file_system import CompiledFileSystem
from object_store_creator import ObjectStoreCreator
from permissions_data_source import PermissionsDataSource
from test_file_system import TestFileSystem

class FakeTemplateDataSource(object):
  class Factory():
    def Create(self, *args):
      return FakeTemplateDataSource()
  def get(self, key):
    return 'partial ' + key

file_system = TestFileSystem({
  'permissions.json': json.dumps({
    'host-permissions': {
      'name': 'match pattern',
      'anchor': 'custom-anchor',
      'platforms': ['app', 'extension'],
      'partial': 'host_permissions.html',
      'literal_name': True
    },
    'activeTab': {
      'partial': 'active_tab.html'
    },
    'alarms': {
      'partial': 'alarms.html'
    },
    'audioCapture': {
      'partial': 'audio_capture.html'
    },
    'background': {
      'partial': 'background.html'
    }
  }),
  '_permission_features.json': json.dumps({
    'activeTab': {
      'extension_types': ['extension', 'packaged_app'],
    },
    'alarms': {
      'extension_types': ['extension', 'packaged_app', 'platform_app'],
    },
      'audioCapture': {
      'extension_types': ['platform_app']
    },
    'background': {
      'extension_types': ['extension', 'packaged_app', 'hosted_app']
    },
    'commandLinePrivate': {
      'extension_types': 'all'
    },
    'cookies': {
      'extension_types': ['platform_app']
    }
  }),
  '_api_features.json': json.dumps({
    'cookies': {
      'dependencies': ['permission:cookies']
    }
  })
})

class PermissionsDataSourceTest(unittest.TestCase):
  def testCreatePermissionsDataSource(self):
    expected_extensions = [
      {
        'name': 'activeTab',
        'anchor': 'activeTab',
        'platforms': ['extension'],
        'description': 'partial active_tab.html'
      },
      {
        'name': 'alarms',
        'anchor': 'alarms',
        'platforms': ['app', 'extension'],
        'description': 'partial alarms.html'
      },
      {
        'name': 'background',
        'anchor': 'background',
        'platforms': ['extension'],
        'description': 'partial background.html'
      },
      {
        'name': 'match pattern',
        'anchor': 'custom-anchor',
        'literal_name': True,
        'description': 'partial host_permissions.html',
        'platforms': ['app', 'extension']
      }
    ]

    expected_apps = [
      {
        'name': 'alarms',
        'anchor': 'alarms',
        'platforms': ['app', 'extension'],
        'description': 'partial alarms.html'
      },
      {
        'name': 'audioCapture',
        'anchor': 'audioCapture',
        'description': 'partial audio_capture.html',
        'platforms': ['app']
      },
      {
        'anchor': 'cookies',
        'name': 'cookies',
        'description': 'partial permissions/generic_description',
        'platforms': ['app']
      },
      {
        'name': 'match pattern',
        'anchor': 'custom-anchor',
        'literal_name': True,
        'description': 'partial host_permissions.html',
        'platforms': ['app', 'extension']
      }
    ]

    permissions_data_source = PermissionsDataSource(
        CompiledFileSystem.Factory(file_system, ObjectStoreCreator.ForTest()),
        file_system,
        '_api_features.json',
        '_permission_features.json',
        'permissions.json')

    permissions_data_source.SetTemplateDataSource(
        FakeTemplateDataSource.Factory())

    self.assertEqual(
        expected_extensions,
        permissions_data_source.get('declare_extensions'))
    self.assertEqual(
        expected_apps,
        permissions_data_source.get('declare_apps'))

if __name__ == '__main__':
  unittest.main()
