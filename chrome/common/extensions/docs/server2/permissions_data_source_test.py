#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from compiled_file_system import CompiledFileSystem
from features_bundle import FeaturesBundle
from object_store_creator import ObjectStoreCreator
from permissions_data_source import PermissionsDataSource
from test_file_system import TestFileSystem

class _FakeTemplateDataSource(object):
  class Factory():
    def Create(self, *args):
      return _FakeTemplateDataSource()
  def get(self, key):
    return 'partial ' + key


_TEST_PERMISSION_FEATURES = {
  'host-permissions': {
    'name': 'match pattern',
    'anchor': 'custom-anchor',
    'platforms': ['apps', 'extensions'],
    'partial': 'host_permissions.html',
    'literal_name': True
  },
  'activeTab': {
    'name': 'activeTab',
    'platforms': ['extensions'],
    'partial': 'active_tab.html'
  },
  'alarms': {
    'name': 'alarms',
    'platforms': ['apps', 'extensions'],
    'partial': 'alarms.html'
  },
  'audioCapture': {
    'name': 'audioCapture',
    'platforms': ['apps'],
    'partial': 'audio_capture.html'
  },
  'background': {
    'name': 'background',
    'platforms': ['extensions'],
    'partial': 'background.html'
  },
  'commandLinePrivate': {
    'name': 'commandLinePrivate',
    'platforms': ['apps', 'extensions']
  },
  'cookies': {
    'name': 'cookies',
    'platforms': ['apps']
  }
}

_TEST_API_FEATURES = {
  'cookies': {
    'dependencies': ['permission:cookies']
  },
  'alarms': {
    'dependencies': ['permission:alarms']
  }
}


class _FakeFeaturesBundle(object):
  def GetAPIFeatures(self):
    return _TEST_API_FEATURES

  def GetPermissionFeatures(self):
    return _TEST_PERMISSION_FEATURES


class _FakeServerInstance(object):
  def __init__(self):
    self.features_bundle = _FakeFeaturesBundle()
    self.object_store_creator = ObjectStoreCreator.ForTest()


class PermissionsDataSourceTest(unittest.TestCase):
  def testCreatePermissionsDataSource(self):
    expected_extensions = [
      {
        'name': 'activeTab',
        'anchor': 'activeTab',
        'platforms': ['extensions'],
        'description': 'partial active_tab.html'
      },
      {
        'name': 'alarms',
        'anchor': 'alarms',
        'platforms': ['apps', 'extensions'],
        'description': 'partial alarms.html'
      },
      {
        'name': 'background',
        'anchor': 'background',
        'platforms': ['extensions'],
        'description': 'partial background.html'
      },
      {
        'name': 'match pattern',
        'anchor': 'custom-anchor',
        'literal_name': True,
        'description': 'partial host_permissions.html',
        'platforms': ['apps', 'extensions']
      }
    ]

    expected_apps = [
      {
        'name': 'alarms',
        'anchor': 'alarms',
        'platforms': ['apps', 'extensions'],
        'description': 'partial alarms.html'
      },
      {
        'name': 'audioCapture',
        'anchor': 'audioCapture',
        'description': 'partial audio_capture.html',
        'platforms': ['apps']
      },
      {
        'anchor': 'cookies',
        'name': 'cookies',
        'description': 'partial permissions/generic_description',
        'platforms': ['apps']
      },
      {
        'name': 'match pattern',
        'anchor': 'custom-anchor',
        'literal_name': True,
        'description': 'partial host_permissions.html',
        'platforms': ['apps', 'extensions']
      }
    ]

    permissions_data_source = PermissionsDataSource(_FakeServerInstance())
    permissions_data_source.SetTemplateDataSource(
        _FakeTemplateDataSource.Factory())

    self.assertEqual(
        expected_extensions,
        permissions_data_source.get('declare_extensions'))
    self.assertEqual(
        expected_apps,
        permissions_data_source.get('declare_apps'))

if __name__ == '__main__':
  unittest.main()
