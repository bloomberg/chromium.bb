#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of shared_prefs.py (mostly SharedPrefs).
"""

import logging
import os
import sys
import unittest

from pylib import constants
from pylib.device import device_utils
from pylib.device import shared_prefs

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock


def MockDeviceWithFiles(files=None):
  if files is None:
    files = {}

  def file_exists(path):
    return path in files

  def write_file(path, contents, **_kwargs):
    files[path] = contents

  def read_file(path, **_kwargs):
    return files[path]

  device = mock.MagicMock(spec=device_utils.DeviceUtils)
  device.FileExists = mock.Mock(side_effect=file_exists)
  device.WriteFile = mock.Mock(side_effect=write_file)
  device.ReadFile = mock.Mock(side_effect=read_file)
  return device


class SharedPrefsTest(unittest.TestCase):

  def setUp(self):
    self.device = MockDeviceWithFiles({
      '/data/data/com.some.package/shared_prefs/prefs.xml':
          "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
          '<map>\n'
          '  <int name="databaseVersion" value="107" />\n'
          '  <boolean name="featureEnabled" value="false" />\n'
          '  <string name="someHashValue">249b3e5af13d4db2</string>\n'
          '</map>'})
    self.expected_data = {'int:databaseVersion': 107,
                          'boolean:featureEnabled': False,
                          'string:someHashValue': '249b3e5af13d4db2'}

  def testMutableMappingInterface(self):
    prefs = shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml')
    self.assertEquals(len(prefs), 0) # collection is empty before loading
    prefs['int:myValue'] = 444
    self.assertEquals(prefs['int:myValue'], 444)
    self.assertTrue('int:myValue' in prefs)
    del prefs['int:myValue']
    self.assertFalse('int:myValue' in prefs)
    with self.assertRaises(KeyError):
      if prefs['int:myValue'] == 444:
        assert False
    with self.assertRaises(TypeError):
      prefs['a_random_key'] = 123
    with self.assertRaises(TypeError):
      prefs['blob:stuff'] = 123

  def testLoad(self):
    prefs = shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml')
    self.assertEquals(len(prefs), 0) # collection is empty before loading
    prefs.Load()
    self.assertEquals(len(prefs), len(self.expected_data))
    self.assertEquals(prefs.AsDict(), self.expected_data)
    self.assertFalse(prefs.changed)

  def testClear(self):
    prefs = shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml')
    prefs.Load()
    self.assertEquals(prefs.AsDict(), self.expected_data)
    self.assertFalse(prefs.changed)
    prefs.Clear()
    self.assertEquals(len(prefs), 0) # collection is empty now
    self.assertTrue(prefs.changed)

  def testCommit(self):
    sample_data = {'int:magicNumber': 42,
                   'float:myMetric': 3.14,
                   'long:bigNumner': 6000000000,
                   'set:apps': ['gmail', 'chrome', 'music']}
    prefs = shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'other_prefs.xml')
    self.assertFalse(self.device.FileExists(prefs.path)) # file does not exist
    prefs.Load()
    self.assertEquals(len(prefs), 0) # file did not exist, collection is empty
    prefs.update(sample_data)
    self.assertEquals(len(prefs), len(sample_data))
    self.assertFalse(self.device.FileExists(prefs.path)) # still does not exist
    self.assertTrue(prefs.changed)
    prefs.Commit()
    self.assertTrue(self.device.FileExists(prefs.path)) # should exist now
    self.device.KillAll.assert_called_once_with(prefs.package, as_root=True,
                                                quiet=True)
    self.assertFalse(prefs.changed)

    prefs = shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'other_prefs.xml')
    self.assertEquals(len(prefs), 0) # collection is empty before loading
    prefs.Load()
    self.assertEquals(prefs.AsDict(), sample_data) # data survived roundtrip

  def testAsContextManager_onlyReads(self):
    with shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml') as prefs:
      self.assertEquals(prefs.AsDict(), self.expected_data) # loaded and ready
    self.assertEquals(self.device.WriteFile.call_args_list, []) # did not write

  def testAsContextManager_readAndWrite(self):
    with shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml') as prefs:
      prefs['boolean:featureEnabled'] = True
      del prefs['string:someHashValue']
      prefs['string:newString'] = 'hello'

    self.assertTrue(self.device.WriteFile.called) # did write
    with shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml') as prefs:
      # changes persisted
      self.assertTrue(prefs['boolean:featureEnabled'])
      self.assertFalse('string:someHashValue' in prefs)
      self.assertEquals(prefs['string:newString'], 'hello')
      self.assertTrue('int:databaseVersion' in prefs) # still there

  def testAsContextManager_commitAborted(self):
    with self.assertRaises(TypeError):
      with shared_prefs.SharedPrefs(
          self.device, 'com.some.package', 'prefs.xml') as prefs:
        prefs['boolean:featureEnabled'] = True
        del prefs['string:someHashValue']
        prefs['string:newString'] = 'hello'
        prefs['a_random_key'] = 'oops!'

    self.assertEquals(self.device.WriteFile.call_args_list, []) # did not write
    with shared_prefs.SharedPrefs(
        self.device, 'com.some.package', 'prefs.xml') as prefs:
      # contents were not modified
      self.assertEquals(prefs.AsDict(), self.expected_data)

if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
