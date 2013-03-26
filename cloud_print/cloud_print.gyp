# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,

    'variables': {
      'version_py_path': '../tools/build/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path) -f',
    'version_path': '<(version_path)',
  },
  'includes': [
    '../chrome/version.gypi',
  ],
  'targets': [
    {
      'target_name': 'cloud_print_version_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'cloud_print',
        'template_input_path': '../chrome/app/chrome_version.rc.version', 
        'extra_variable_files_arguments': [ '-f', 'BRANDING' ],
        'extra_variable_files': [ 'BRANDING' ], # NOTE: matches that above
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)',
        ],
      },
      'sources': [
        'service/win/cloud_print_service_exe.ver',
        'virtual_driver/win/gcp_portmon64_dll.ver',
        'virtual_driver/win/gcp_portmon_dll.ver',
        'virtual_driver/win/install/virtual_driver_setup_exe.ver',
      ],
      'includes': [
        '../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'cloud_print',
      'type': 'none',
      'dependencies': [
        'service/service.gyp:*',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
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
