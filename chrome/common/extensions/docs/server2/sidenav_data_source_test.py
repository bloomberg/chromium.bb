#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from extensions_paths import JSON_TEMPLATES
from mock_file_system import MockFileSystem
from server_instance import ServerInstance
from servlet import Request
from sidenav_data_source import SidenavDataSource, _AddLevels, _AddSelected
from test_file_system import TestFileSystem
from test_util import CaptureLogging


class SamplesDataSourceTest(unittest.TestCase):
  def testAddLevels(self):
    sidenav_json = [{
      'title': 'H2',
      'items': [{
        'title': 'H3',
        'items': [{ 'title': 'X1' }]
      }]
    }]

    expected = [{
      'level': 1,
      'title': 'H2',
      'items': [{
        'level': 2,
        'title': 'H3',
        'items': [{ 'level': 3, 'title': 'X1' }]
      }]
    }]

    _AddLevels(sidenav_json, 1)
    self.assertEqual(expected, sidenav_json)

  def testAddSelected(self):
    sidenav_json = [
      { 'href': '/AH2.html' },
      {
        'href': '/H2.html',
        'items': [{
          'href': '/H3.html'
        }]
      }
    ]

    expected = [
      { 'href': '/AH2.html' },
      {
        'child_selected': True,
        'href': '/H2.html',
        'items': [{
          'href': '/H3.html',
          'selected': True
        }]
      }
    ]

    _AddSelected(sidenav_json, '/H3.html')
    self.assertEqual(expected, sidenav_json)

  def testWithDifferentBasePath(self):
    file_system = TestFileSystem({
      'apps_sidenav.json': json.dumps([
        { 'href': '/H1.html' },
        { 'href': '/H2.html' },
        { 'href': '/base/path/H2.html' },
        { 'href': 'https://qualified/X1.html' },
        {
          'href': 'H3.html',
          'items': [{
            'href': 'H4.html'
          }]
        },
      ])
    }, relative_to=JSON_TEMPLATES)

    expected = [
      {'href': '/base/path/H1.html', 'level': 2},
      {'href': '/base/path/H2.html', 'level': 2, 'selected': True},
      {'href': '/base/path/base/path/H2.html', 'level': 2},
      {'href': 'https://qualified/X1.html', 'level': 2},
      {'items': [
        {'href': '/base/path/H4.html', 'level': 3}
      ],
      'href': '/base/path/H3.html', 'level': 2}
    ]

    server_instance = ServerInstance.ForTest(file_system,
                                             base_path='/base/path/')
    sidenav_data_source = SidenavDataSource(server_instance,
                                            Request.ForTest('/H2.html'))

    log_output = CaptureLogging(
        lambda: self.assertEqual(expected, sidenav_data_source.get('apps')))
    self.assertEqual(2, len(log_output))

  def testSidenavDataSource(self):
    file_system = MockFileSystem(TestFileSystem({
      'apps_sidenav.json': json.dumps([{
        'title': 'H1',
        'href': 'H1.html',
        'items': [{
          'title': 'H2',
          'href': '/H2.html'
        }]
      }])
    }, relative_to=JSON_TEMPLATES))

    expected = [{
      'level': 2,
      'child_selected': True,
      'title': 'H1',
      'href': '/H1.html',
      'items': [{
        'level': 3,
        'selected': True,
        'title': 'H2',
        'href': '/H2.html'
      }]
    }]

    sidenav_data_source = SidenavDataSource(
        ServerInstance.ForTest(file_system), Request.ForTest('/H2.html'))
    self.assertTrue(*file_system.CheckAndReset())

    log_output = CaptureLogging(
        lambda: self.assertEqual(expected, sidenav_data_source.get('apps')))

    self.assertEqual(1, len(log_output))
    self.assertTrue(
        log_output[0].msg.startswith('Paths in sidenav must be qualified.'))

    # Test that only a single file is read when creating the sidenav, so that
    # we can be confident in the compiled_file_system.SingleFile annotation.
    self.assertTrue(*file_system.CheckAndReset(
        read_count=1, stat_count=1, read_resolve_count=1))

  def testCron(self):
    file_system = TestFileSystem({
      'apps_sidenav.json': '[{ "title": "H1" }]' ,
      'extensions_sidenav.json': '[{ "title": "H2" }]'
    }, relative_to=JSON_TEMPLATES)

    # Ensure Cron doesn't rely on request.
    sidenav_data_source = SidenavDataSource(
        ServerInstance.ForTest(file_system), request=None)
    sidenav_data_source.Cron().Get()

    # If Cron fails, apps_sidenav.json will not be cached, and the _cache_data
    # access will fail.
    # TODO(jshumway): Make a non hack version of this check.
    sidenav_data_source._cache._file_object_store.Get(
        '%s/apps_sidenav.json' % JSON_TEMPLATES).Get()._cache_data


if __name__ == '__main__':
  unittest.main()
