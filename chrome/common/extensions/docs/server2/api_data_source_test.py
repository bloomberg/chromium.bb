#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from api_data_source import (_JSCModel,
                             _FormatValue,
                             _GetEventByNameFromEvents)
from branch_utility import ChannelInfo
from extensions_paths import CHROME_EXTENSIONS
from fake_host_file_system_provider import FakeHostFileSystemProvider
from features_bundle import FeaturesBundle
from file_system import FileNotFoundError
from future import Future
from object_store_creator import ObjectStoreCreator
from server_instance import ServerInstance
from test_data.canned_data import (CANNED_API_FILE_SYSTEM_DATA, CANNED_BRANCHES)
from test_data.api_data_source.canned_trunk_fs import CANNED_TRUNK_FS_DATA
from test_file_system import TestFileSystem
from test_util import Server2Path
from third_party.json_schema_compiler.memoize import memoize


def _MakeLink(href, text):
  return '<a href="%s">%s</a>' % (href, text)


def _GetType(dict_, name):
  for type_ in dict_['types']:
    if type_['name'] == name:
      return type_


class _FakeAvailabilityFinder(object):

  def GetApiAvailability(self, version):
    return ChannelInfo('stable', '396', 5)


class _FakeTemplateCache(object):

  def GetFromFile(self, key):
    return Future(value='handlebar %s' % key)


class APIDataSourceTest(unittest.TestCase):

  def setUp(self):
    self._base_path = Server2Path('test_data', 'test_json')

    server_instance = ServerInstance.ForTest(
        TestFileSystem(CANNED_TRUNK_FS_DATA, relative_to=CHROME_EXTENSIONS))
    file_system = server_instance.host_file_system_provider.GetTrunk()
    self._json_cache = server_instance.compiled_fs_factory.ForJson(file_system)
    self._features_bundle = FeaturesBundle(file_system,
                                           server_instance.compiled_fs_factory,
                                           server_instance.object_store_creator)
    self._api_models = server_instance.api_models

    # Used for testGetApiAvailability() so that valid-ish data is processed.
    server_instance = ServerInstance.ForTest(
        file_system_provider=FakeHostFileSystemProvider(
            CANNED_API_FILE_SYSTEM_DATA))
    self._avail_api_models = server_instance.api_models
    self._avail_json_cache = server_instance.compiled_fs_factory.ForJson(
        server_instance.host_file_system_provider.GetTrunk())
    self._avail_finder = server_instance.availability_finder

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _LoadJSON(self, filename):
    return json.loads(self._ReadLocalFile(filename))

  def testCreateId(self):
    dict_ = _JSCModel('tester',
                      self._api_models,
                      _FakeAvailabilityFinder(),
                      self._json_cache,
                      _FakeTemplateCache(),
                      self._features_bundle,
                      None).ToDict()
    self.assertEquals('type-TypeA', dict_['types'][0]['id'])
    self.assertEquals('property-TypeA-b',
                      dict_['types'][0]['properties'][0]['id'])
    self.assertEquals('method-get', dict_['functions'][0]['id'])
    self.assertEquals('event-EventA', dict_['events'][0]['id'])

  # TODO(kalman): re-enable this when we have a rebase option.
  def DISABLED_testToDict(self):
    expected_json = self._LoadJSON('expected_tester.json')
    dict_ = _JSCModel('tester',
                      self._api_models,
                      _FakeAvailabilityFinder(),
                      self._json_cache,
                      _FakeTemplateCache(),
                      self._features_bundle,
                      None).ToDict()
    self.assertEquals(expected_json, dict_)

  def testFormatValue(self):
    self.assertEquals('1,234,567', _FormatValue(1234567))
    self.assertEquals('67', _FormatValue(67))
    self.assertEquals('234,567', _FormatValue(234567))

  def testGetApiAvailability(self):
    api_availabilities = {
      'bluetooth': ChannelInfo('dev', CANNED_BRANCHES[28], 28),
      'contextMenus': ChannelInfo('trunk', CANNED_BRANCHES['trunk'], 'trunk'),
      'jsonStableAPI': ChannelInfo('stable', CANNED_BRANCHES[20], 20),
      'idle': ChannelInfo('stable', CANNED_BRANCHES[5], 5),
      'input.ime': ChannelInfo('stable', CANNED_BRANCHES[18], 18),
      'tabs': ChannelInfo('stable', CANNED_BRANCHES[18], 18)
    }
    for api_name, availability in api_availabilities.iteritems():
      model = _JSCModel(api_name,
                        self._avail_api_models,
                        self._avail_finder,
                        self._avail_json_cache,
                        _FakeTemplateCache(),
                        self._features_bundle,
                        None)
      self.assertEquals(availability, model._GetApiAvailability())

  def testGetIntroList(self):
    model = _JSCModel('tester',
                      self._api_models,
                      _FakeAvailabilityFinder(),
                      self._json_cache,
                      _FakeTemplateCache(),
                      self._features_bundle,
                      None)
    expected_list = [
      { 'title': 'Description',
        'content': [
          { 'text': 'a test api' }
        ]
      },
      { 'title': 'Availability',
        'content': [
          { 'partial': 'handlebar chrome/common/extensions/docs/' +
                       'templates/private/intro_tables/stable_message.html',
            'version': 5
          }
        ]
      },
      { 'title': 'Permissions',
        'content': [
          { 'class': 'override',
            'text': '"tester"'
          },
          { 'text': 'is an API for testing things.' }
        ]
      },
      { 'title': 'Manifest',
        'content': [
          { 'class': 'code',
            'text': '"tester": {...}'
          }
        ]
      },
      { 'title': 'Learn More',
        'content': [
          { 'link': 'https://tester.test.com/welcome.html',
            'text': 'Welcome!'
          }
        ]
      }
    ]
    self.assertEquals(model._GetIntroTableList(), expected_list)

  def testGetEventByNameFromEvents(self):
    events = {}
    # Missing 'types' completely.
    self.assertRaises(AssertionError, _GetEventByNameFromEvents, events)

    events['types'] = []
    # No type 'Event' defined.
    self.assertRaises(AssertionError, _GetEventByNameFromEvents, events)

    events['types'].append({ 'name': 'Event',
                             'functions': []})
    add_rules = { "name": "addRules" }
    events['types'][0]['functions'].append(add_rules)
    self.assertEqual(add_rules,
                     _GetEventByNameFromEvents(events)['addRules'])

    events['types'][0]['functions'].append(add_rules)
    # Duplicates are an error.
    self.assertRaises(AssertionError, _GetEventByNameFromEvents, events)

  def _FakeLoadAddRulesSchema(self):
    events = self._LoadJSON('add_rules_def_test.json')
    return _GetEventByNameFromEvents(events)

  def testAddRules(self):
    dict_ = _JSCModel('add_rules_tester',
                      self._api_models,
                      _FakeAvailabilityFinder(),
                      self._json_cache,
                      _FakeTemplateCache(),
                      self._features_bundle,
                      self._FakeLoadAddRulesSchema).ToDict()

    # Check that the first event has the addRulesFunction defined.
    self.assertEquals('add_rules_tester', dict_['name'])
    self.assertEquals('rules', dict_['events'][0]['name'])
    self.assertEquals('notable_name_to_check_for',
                      dict_['events'][0]['byName']['addRules'][
                          'parameters'][0]['name'])

    # Check that the second event has addListener defined.
    self.assertEquals('noRules', dict_['events'][1]['name'])
    self.assertEquals('add_rules_tester', dict_['name'])
    self.assertEquals('noRules', dict_['events'][1]['name'])
    self.assertEquals('callback',
                      dict_['events'][0]['byName']['addListener'][
                          'parameters'][0]['name'])

if __name__ == '__main__':
  unittest.main()
