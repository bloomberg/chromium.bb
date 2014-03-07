#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from api_categorizer import APICategorizer
from compiled_file_system import CompiledFileSystem
from extensions_paths import CHROME_EXTENSIONS
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem


def _ToTestData(obj):
  '''Transforms |obj| into test data by turning a list of files into an object
  mapping that file to its contents (derived from its name).
  '''
  return dict((name, name) for name in obj)


_TEST_DATA = {
  'api': {
    '_api_features.json': '{}',
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
  }
}


class APICategorizerTest(unittest.TestCase):
  def setUp(self):
    self._api_categorizer = APICategorizer(
        TestFileSystem(_TEST_DATA, relative_to=CHROME_EXTENSIONS),
        CompiledFileSystem.Factory(ObjectStoreCreator.ForTest()))

  def testGetAPICategory(self):
    get_category = self._api_categorizer.GetCategory
    self.assertEqual('chrome', get_category('apps', 'alarms'))
    self.assertEqual('chrome', get_category('extensions', 'alarms'))
    self.assertEqual('private', get_category('apps', 'musicManagerPrivate'))
    self.assertEqual('private', get_category('extensions', 'notDocumentedApi'))
    self.assertEqual('experimental',
                     get_category('apps', 'experimental.bluetooth'))
    self.assertEqual('experimental',
                     get_category('extensions', 'experimental.history'))


if __name__ == '__main__':
  unittest.main()