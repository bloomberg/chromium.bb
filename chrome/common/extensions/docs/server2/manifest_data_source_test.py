#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from compiled_file_system import CompiledFileSystem
from manifest_data_source import ManifestDataSource
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem

file_system = TestFileSystem({
  "_manifest_features.json": json.dumps({
    'req0': {
      'extension_types': ['platform_app', 'extension']
    },
    'req1': {
      'extension_types': 'all'
    },
    'opt0': {
      'extension_types': ['extension']
    },
    'opt1': {
      'extension_types': ['hosted_app']
    },
    'free0': {
      'extension_types': ['platform_app']
    },
    'free1': {
      'extension_types': ['platform_app', 'hosted_app', 'extension']
    },
    'only0': {
      'extension_types': 'all'
    },
    'only1': {
      'extension_types': ['platform_app']
    },
    'rec0': {
    'extension_types': ['extension']
    },
    'rec1': {
      'extension_types': ['platform_app', 'extension']
    }
  }),
  "manifest.json": json.dumps({
    'required': [
      {
        'name': 'req0',
        'example': 'Extension'
      },
      {'name': 'req1'}
    ],
    'only_one': [
      {'name': 'only0'},
      {'name': 'only1'}
    ],
    'recommended': [
      {'name': 'rec0'},
      {'name': 'rec1'}
    ],
    'optional': [
      {'name': 'opt0'},
      {'name': 'opt1'}
    ]
  })
})

class ManifestDataSourceTest(unittest.TestCase):
  def testCreateManifestData(self):
    expected_extensions = {
      'required': [
        {
          'name': 'req0',
          'example': 'Extension'
        },
        {'name': 'req1'}
      ],
      'recommended': [
        {'name': 'rec0'},
        {'name': 'rec1'}
      ],
      'only_one': [
        {'name': 'only0'}
      ],
      'optional': [
        {'name': 'free1'},
        {
          'name': 'opt0',
          'is_last': True
        }
      ]
    }

    expected_apps = {
      'required': [
        {
          'name': 'req0',
          'example': 'Application'
        },
        {'name': 'req1'}
      ],
      'recommended': [
        {'name': 'rec1'}
      ],
      'only_one': [
        {'name': 'only0'},
        {'name': 'only1'}
      ],
      'optional': [
        {'name': 'free0'},
        {
          'name': 'free1',
          'is_last': True
        }
      ]
    }

    mds = ManifestDataSource(
        CompiledFileSystem.Factory(file_system, ObjectStoreCreator.ForTest()),
        file_system, 'manifest.json', '_manifest_features.json')

    self.assertEqual(expected_extensions, mds.get('extensions'))
    self.assertEqual(expected_apps, mds.get('apps'))

if __name__ == '__main__':
  unittest.main()
