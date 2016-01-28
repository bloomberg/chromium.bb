# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //cloud_print
      'target_name': 'cloud_print',
      'type': 'none',
      'dependencies': [
        'gcp20/prototype/gcp20_device.gyp:*',
        'service/service.gyp:*',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'service/win/service.gyp:*',
            'virtual_driver/win/install/virtual_driver_install.gyp:*',
            'virtual_driver/win/virtual_driver.gyp:*',
          ],
        }],
        ['OS=="win" and target_arch=="ia32"', {
          'dependencies': [
            'virtual_driver/win/virtual_driver64.gyp:*',
          ],
        }],
      ],
    },
    {
      # GN version: //cloud_print:cloud_print_unittests
      'target_name': 'cloud_print_unittests',
      'type': 'executable',
      'sources': [
        'service/service_state_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'service/service.gyp:cloud_print_service_lib',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'service/win/service_ipc_unittest.cc',
            'virtual_driver/win/port_monitor/port_monitor_unittest.cc',
          ],
          'dependencies': [
            'virtual_driver/win/virtual_driver.gyp:gcp_portmon_lib',
          ],
        }],
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
  ],
}
