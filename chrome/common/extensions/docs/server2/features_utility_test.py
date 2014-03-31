#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from features_utility import Parse, Filtered, MergedWith


class FeaturesUtilityTest(unittest.TestCase):
  def testFromJson(self):
    raw_features_json = {
      'doc1': {
        'extension_types': ['extension', 'platform_app']
      },
      'doc2': {
        'extension_types': ['hosted_app', 'packaged_app']
      },
      'doc3': {
        'whitelist': 'hashhashashhashashhashashhash'
      },
      'doc4': [
        { 'extension_types': 'all' },
        { 'whitelist': 'hashhashashhashashhashashhash' }
      ],
      'doc5': {
        'extension_types': ['extension']
      },
      'doc1.sub1': {
        'extension_types': ['platform_app', 'hosted_app', 'packaged_app']
      }
    }

    expected = {
      'doc1': {
        'platforms': ['apps', 'extensions'],
        'name': 'doc1'
      },
      'doc2': {
        'platforms': [],
        'name': 'doc2'
      },
      'doc4': {
        'platforms': ['apps', 'extensions'],
        'name': 'doc4'
      },
      'doc5': {
        'platforms': ['extensions'],
        'name': 'doc5'
      },
      'doc1.sub1': {
        'platforms': ['apps'],
        'name': 'doc1.sub1'
      }
    }

    self.assertEqual(expected, Parse(raw_features_json))

  def testFeatureList(self):
    raw_features_json = {
      'doc1': [
        { 'extension_types': ['extension'] },
        { 'extension_types': ['platform_app'] }
      ],
      'doc2': [
        { 'channel': 'dev', 'extension_types': ['extension', 'platform_app'] },
        { 'channel': 'stable' }
      ],
      'doc3': [
        { 'channel': 'beta' },
        { 'channel': 'dev' }
      ],
      'doc4': [
        { 'channel': 'beta' },
        { 'dependencies': ['permission:perm1'] }
      ]
    }

    expected = {
      'doc1': {
        'platforms': ['apps', 'extensions'],
        'name': 'doc1'
      },
      'doc2': {
        'channel': 'stable',
        'platforms': ['apps', 'extensions'],
        'name': 'doc2'
      },
      'doc3': {
        'platforms': [],
        'channel': 'beta',
        'name': 'doc3'
      },
      'doc4': {
        'platforms': [],
        'dependencies': ['permission:perm1'],
        'name': 'doc4'
      }
    }

    self.assertEqual(expected, Parse(raw_features_json))

  def testFilter(self):
    unfiltered = {
      'doc1': { 'platforms': ['apps'] },
      'doc2': { 'platforms': ['extensions'] },
      'doc3': { 'platforms': ['apps', 'extensions'] },
      'doc4': { 'platforms': [] }
    }

    apps_names = set(('doc1', 'doc3'))
    extension_names = set(('doc2', 'doc3'))

    self.assertEqual(sorted(apps_names),
                     sorted(Filtered(unfiltered, 'apps').keys()))
    self.assertEqual(sorted(extension_names),
                     sorted(Filtered(unfiltered, 'extensions').keys()))

  def testMergeFeatures(self):
    features = {
      'doc1': {
        'platforms': ['apps']
      },
      'doc3': {
        'name': 'doc3'
      }
    }

    other = {
      'doc1': {
        'name': 'doc1',
        'platforms': ['extensions']
      },
      'doc2': {
        'name': 'doc2'
      },
      'doc3': {
        'platforms': ['extensions', 'apps']
      }
    }

    expected = {
      'doc1': {
        'name': 'doc1',
        'platforms': ['extensions']
      },
      'doc2': {
        'name': 'doc2',
        'platforms': []
      },
      'doc3': {
        'name': 'doc3',
        'platforms': ['extensions', 'apps']
      }
    }

    self.assertEqual(expected, MergedWith(features, other))

if __name__ == '__main__':
  unittest.main()
