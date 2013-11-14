#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from extensions_paths import EXTENSIONS
from server_instance import ServerInstance
from test_file_system import TestFileSystem


_TEST_FILESYSTEM = {
  'api': {
    '_api_features.json': json.dumps({
      'audioCapture': {
        'channel': 'stable',
        'extension_types': ['platform_app']
      },
      'background': [
        {
          'channel': 'stable',
          'extension_types': ['extension']
        },
        {
          'channel': 'stable',
          'extension_types': ['platform_app'],
          'whitelist': ['im not here']
        }
      ],
      'omnibox': {
        'dependencies': ['manifest:omnibox'],
        'contexts': ['blessed_extension']
      },
      'syncFileSystem': {
        'dependencies': ['permission:syncFileSystem'],
        'contexts': ['blessed_extension']
      },
      'tabs': {
        'channel': 'stable',
        'extension_types': ['extension', 'legacy_packaged_app'],
        'contexts': ['blessed_extension']
      },
      'test': {
        'channel': 'stable',
        'extension_types': 'all',
        'contexts': [
            'blessed_extension', 'unblessed_extension', 'content_script']
      },
      'windows': {
        'dependencies': ['api:tabs'],
        'contexts': ['blessed_extension']
      }
    }),
    '_manifest_features.json': json.dumps({
      'app.content_security_policy': {
        'channel': 'stable',
        'extension_types': ['platform_app'],
        'min_manifest_version': 2,
        'whitelist': ['this isnt happening']
      },
      'background': {
        'channel': 'stable',
        'extension_types': ['extension', 'legacy_packaged_app', 'hosted_app']
      },
      'manifest_version': {
        'channel': 'stable',
        'extension_types': 'all'
      },
      'omnibox': {
        'channel': 'stable',
        'extension_types': ['extension']
      },
      'page_action': {
        'channel': 'stable',
        'extension_types': ['extension']
      },
      'sockets': {
        'channel': 'dev',
        'extension_types': ['platform_app']
      }
    }),
    '_permission_features.json': json.dumps({
      'bluetooth': {
        'channel': 'dev',
        'extension_types': ['platform_app']
      },
      'power': {
        'channel': 'stable',
        'extension_types': [
          'extension', 'legacy_packaged_app', 'platform_app'
        ]
      },
      'syncFileSystem': {
        'channel': 'stable',
        'extension_types': ['platform_app']
      },
      'tabs': {
        'channel': 'stable',
        'extension_types': ['extension']
      }
    })
  },
  'docs': {
    'templates': {
      'json': {
        'manifest.json': json.dumps({
          'background': {
            'documentation': 'background_pages.html'
          },
          'manifest_version': {
            'documentation': 'manifest/manifest_version.html',
            'example': 2,
            'level': 'required'
          },
          'page_action': {
            'documentation': 'pageAction.html',
            'example': {},
            'level': 'only_one'
          }
        }),
        'permissions.json': json.dumps({
          'fakeUnsupportedFeature': {},
          'syncFileSystem': {
            'partial': 'permissions/sync_file_system.html'
          },
          'tabs': {
            'partial': 'permissions/tabs.html'
          },
        })
      }
    }
  }
}


class FeaturesBundleTest(unittest.TestCase):
  def setUp(self):
    self._server = ServerInstance.ForTest(
        TestFileSystem(_TEST_FILESYSTEM, relative_to=EXTENSIONS))

  def testManifestFeatures(self):
    expected_features = {
      'background': {
        'name': 'background',
        'channel': 'stable',
        'platforms': ['extensions'],
        'documentation': 'background_pages.html'
      },
      'manifest_version': {
        'name': 'manifest_version',
        'channel': 'stable',
        'platforms': ['apps', 'extensions'],
        'documentation': 'manifest/manifest_version.html',
        'level': 'required',
        'example': 2
      },
      'omnibox': {
        'name': 'omnibox',
        'channel': 'stable',
        'platforms': ['extensions']
      },
      'page_action': {
        'name': 'page_action',
        'channel': 'stable',
        'platforms': ['extensions'],
        'documentation': 'pageAction.html',
        'level': 'only_one',
        'example': {}
      },
      'sockets': {
        'name': 'sockets',
        'channel': 'dev',
        'platforms': ['apps']
      }
    }
    self.assertEqual(
        expected_features,
        self._server.features_bundle.GetManifestFeatures().Get())

  def testPermissionFeatures(self):
    expected_features = {
      'bluetooth': {
        'name': 'bluetooth',
        'channel': 'dev',
        'platforms': ['apps'],
      },
      'fakeUnsupportedFeature': {
        'name': 'fakeUnsupportedFeature',
        'platforms': []
      },
      'power': {
        'name': 'power',
        'channel': 'stable',
        'platforms': ['apps', 'extensions'],
      },
      'syncFileSystem': {
        'name': 'syncFileSystem',
        'channel': 'stable',
        'platforms': ['apps'],
        'partial': 'permissions/sync_file_system.html'
      },
      'tabs': {
        'name': 'tabs',
        'channel': 'stable',
        'platforms': ['extensions'],
        'partial': 'permissions/tabs.html'
      }
    }
    self.assertEqual(
        expected_features,
        self._server.features_bundle.GetPermissionFeatures().Get())

  def testAPIFeatures(self):
    expected_features = {
      'audioCapture': {
        'name': 'audioCapture',
        'channel': 'stable',
        'platforms': ['apps']
      },
      'background': {
        'name': 'background',
        'channel': 'stable',
        'platforms': ['extensions']
      },
      'omnibox': {
        'name': 'omnibox',
        'platforms': ['extensions'],
        'contexts': ['blessed_extension'],
        'dependencies': ['manifest:omnibox']
      },
      'syncFileSystem': {
        'name': 'syncFileSystem',
        'platforms': ['apps'],
        'contexts': ['blessed_extension'],
        'dependencies': ['permission:syncFileSystem']
      },
      'tabs': {
        'name': 'tabs',
        'channel': 'stable',
        'platforms': ['extensions'],
        'contexts': ['blessed_extension'],
      },
      'test': {
        'name': 'test',
        'channel': 'stable',
        'platforms': ['apps', 'extensions'],
        'contexts': [
            'blessed_extension', 'unblessed_extension', 'content_script'],
      },
      'windows': {
        'name': 'windows',
        'platforms': ['extensions'],
        'contexts': ['blessed_extension'],
        'dependencies': ['api:tabs']
      }
    }
    self.assertEqual(
        expected_features,
        self._server.features_bundle.GetAPIFeatures().Get())


if __name__ == '__main__':
  unittest.main()
