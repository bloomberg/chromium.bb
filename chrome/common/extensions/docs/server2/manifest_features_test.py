#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from manifest_features import CreateManifestFeatures, ConvertDottedKeysToNested

class ManifestFeaturesTest(unittest.TestCase):
  def testConvertDottedKeysToNested(self):
    docs = {
      'doc1.sub2': {
        'name': 'doc1.sub2'
      },
      'doc1': {
        'name': 'doc1'
      },
      'doc2': {
        'name': 'doc2'
      },
      'doc1.sub1.subsub1': {
        'name': 'doc1.sub1.subsub1'
      },
      'doc1.sub1': {
        'name': 'doc1.sub1'
      }
    }

    expected_docs = {
      'doc1': {
        'name': 'doc1',
        'children': {
          'sub1': {
            'name': 'sub1',
            'children': {
              'subsub1': {
                'name' :'subsub1'
              }
            }
          },
          'sub2': {
            'name': 'sub2'
          }
        }
      },
      'doc2': {
        'name': 'doc2'
      }
    }

    self.assertEqual(expected_docs, ConvertDottedKeysToNested(docs))

  def testCreateManifestFeatures(self):
    features_json = {
      'doc1': { 'extension_types': 'all' },
      'doc2': { 'extension_types': ['extension', 'package_app'] }
    }

    manifest_json = {
      'doc1': { 'example': {} },
      'doc1.sub1': { 'example': [] }
    }

    expected = {
      'doc1': {
        'name': 'doc1',
        'example': {},
        'platforms': ['app', 'extension']
      },
      'doc1.sub1': {
        'example': [],
        'name': 'doc1.sub1',
        'platforms': []
      },
      'doc2': {
        'name': 'doc2',
        'platforms': ['extension']
      }
    }

    expected_filtered = {
      'doc1': {
        'example': {},
        'name': 'doc1',
        'platforms': ['app', 'extension']
      }
    }

    self.assertEqual(expected, CreateManifestFeatures(
        features_json=features_json,
        manifest_json=manifest_json,
        filter_platform=None))

    self.assertEqual(expected_filtered, CreateManifestFeatures(
        features_json=features_json,
        manifest_json=manifest_json,
        filter_platform='app'))

if __name__ == '__main__':
  unittest.main()
