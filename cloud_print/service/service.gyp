# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
    ],
  },
  'targets': [
    {
      'target_name': 'cloud_print_service_lib',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/printing/printing.gyp:printing',
      ],
      'sources': [
        'service_state.cc',
        'service_state.h',
        'service_switches.cc',
        'service_switches.h',
        'win/chrome_launcher.cc',
        'win/chrome_launcher.h',
        'win/local_security_policy.cc',
        'win/local_security_policy.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:launcher_support',
          ],
        }],
      ],
    },
    {
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'include_dirs': [
        # To allow including "version.h"
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'win/cloud_print_service.cc',
        'win/cloud_print_service.h',
        'win/cloud_print_service.rc',
        'win/resource.h',
      ],
      'dependencies': [
        'cloud_print_service_lib',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
          ],
        }],
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
  ],
}
