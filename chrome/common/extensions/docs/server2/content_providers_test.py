#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from compiled_file_system import CompiledFileSystem
from content_providers import ContentProviders
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_util import DisableLogging


_HOST = 'https://developer.chrome.com'


_CONTENT_PROVIDERS = {
  'apples': {
    'chromium': {
      'dir': 'apples'
    },
    'serveFrom': 'apples-dir',
  },
  'bananas': {
    'serveFrom': '',
    'chromium': {
      'dir': ''
    },
  },
  'tomatoes': {
    'serveFrom': 'tomatoes-dir/are/a',
    'chromium': {
      'dir': 'tomatoes/are/a'
    },
  },
}


_FILE_SYSTEM_DATA = {
  'docs': {
    'templates': {
      'json': {
        'content_providers.json': json.dumps(_CONTENT_PROVIDERS),
      },
    },
  },
  'apples': {
    'gala.txt': 'gala apples',
    'green': {
      'granny smith.txt': 'granny smith apples',
    },
  },
  'tomatoes': {
    'are': {
      'a': {
        'vegetable.txt': 'no they aren\'t',
        'fruit': {
          'cherry.txt': 'cherry tomatoes',
        },
      },
    },
  },
}


class ContentProvidersTest(unittest.TestCase):
  def setUp(self):
    self._content_providers = ContentProviders(
        CompiledFileSystem.Factory(ObjectStoreCreator.ForTest()),
        TestFileSystem(_FILE_SYSTEM_DATA))

  def testSimpleRootPath(self):
    provider = self._content_providers.GetByName('apples')
    self.assertEqual(
        'gala apples',
        provider.GetContentAndType(_HOST, 'gala.txt').Get().content)
    self.assertEqual(
        'granny smith apples',
        provider.GetContentAndType(_HOST, 'green/granny smith.txt').Get()
            .content)

  def testComplexRootPath(self):
    provider = self._content_providers.GetByName('tomatoes')
    self.assertEqual(
        'no they aren\'t',
        provider.GetContentAndType(_HOST, 'vegetable.txt').Get().content)
    self.assertEqual(
        'cherry tomatoes',
        provider.GetContentAndType(_HOST, 'fruit/cherry.txt').Get().content)

  def testEmptyRootPath(self):
    provider = self._content_providers.GetByName('bananas')
    self.assertEqual(
        'gala apples',
        provider.GetContentAndType(_HOST, 'apples/gala.txt').Get().content)

  def testSimpleServlet(self):
    provider, path = self._content_providers.GetByServeFrom('apples-dir')
    self.assertEqual('apples', provider.name)
    self.assertEqual('', path)
    provider, path = self._content_providers.GetByServeFrom(
        'apples-dir/are/forever')
    self.assertEqual('apples', provider.name)
    self.assertEqual('are/forever', path)

  def testComplexServlet(self):
    provider, path = self._content_providers.GetByServeFrom(
        'tomatoes-dir/are/a')
    self.assertEqual('tomatoes', provider.name)
    self.assertEqual('', path)
    provider, path = self._content_providers.GetByServeFrom(
        'tomatoes-dir/are/a/fruit/they/are')
    self.assertEqual('tomatoes', provider.name)
    self.assertEqual('fruit/they/are', path)

  def testEmptyStringServlet(self):
    provider, path = self._content_providers.GetByServeFrom('tomatoes-dir/are')
    self.assertEqual('bananas', provider.name)
    self.assertEqual('tomatoes-dir/are', path)
    provider, path = self._content_providers.GetByServeFrom('')
    self.assertEqual('bananas', provider.name)
    self.assertEqual('', path)

  @DisableLogging('error')
  def testProviderNotFound(self):
    self.assertEqual(None, self._content_providers.GetByName('cabbages'))


if __name__ == '__main__':
  unittest.main()
