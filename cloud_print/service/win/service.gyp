# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
    },
    'include_dirs': [
      '../../..',
    ],
  },
  'targets': [
    {
      'target_name': 'cloud_print_service_lib',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base', 
        '../../../build/temp_gyp/googleurl.gyp:googleurl',
        '../../../net/net.gyp:net',
      ],
      'sources': [
        'chrome_launcher.cc',
        'chrome_launcher.h',
        'service_state.cc',
        'service_state.h',
        'service_switches.cc',
        'service_switches.h',
      ]
    },
    {
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'sources': [
        'cloud_print_service.cc',
        'cloud_print_service.h',
        'cloud_print_service.rc',
        'resource.h',
      ],
      'dependencies': [
        'cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'UACExecutionLevel': '2', # /level='requireAdministrator'
        },
      },
    },
    {
      'target_name': 'cloud_print_service_unittests',
      'type': 'executable',
      'sources': [
        'service_state_unittest.cc',
      ],
      'dependencies': [
        '../../../base/base.gyp:run_all_unittests',
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:test_support_base',
        '../../../testing/gmock.gyp:gmock',
        '../../../testing/gtest.gyp:gtest',
        'cloud_print_service_lib',
      ],
    },
  ],
}
