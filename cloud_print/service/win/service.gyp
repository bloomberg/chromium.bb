# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
      'enable_wexit_time_destructors': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
      # To allow including "version.h"
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'defines' : [
      'COMPILE_CONTENT_STATICALLY',
      'SECURITY_WIN32',
      'STRICT',
      '_ATL_APARTMENT_THREADED',
      '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
      '_ATL_NO_COM_SUPPORT',
      '_ATL_NO_AUTOMATIC_NAMESPACE',
      '_ATL_NO_EXCEPTIONS',
    ],
  },
  'targets': [
    {
      # GN version: //cloud_print/service/win:cloud_print_service
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_exe_version.rc',
        'cloud_print_service.cc',
      ],
      'includes': [
        'service_resources.gypi'
      ],
      'dependencies': [
        '<(DEPTH)/cloud_print/cloud_print_resources.gyp:cloud_print_version_resources',
        '<(DEPTH)/cloud_print/service/service.gyp:cloud_print_service_lib',
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
    {
      # GN version: //cloud_print/service/win:cloud_print_service_config
      'target_name': 'cloud_print_service_config',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_config_exe_version.rc',
        'cloud_print_service_config.cc',
      ],
      'includes': [
        'service_resources.gypi'
      ],
      'dependencies': [
        '<(DEPTH)/cloud_print/cloud_print_resources.gyp:cloud_print_version_resources',
        '<(DEPTH)/cloud_print/common/common.gyp:cloud_print_install_lib',
        '<(DEPTH)/cloud_print/service/service.gyp:cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'common-controls.manifest',
          ],
        },
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
        'conditions': [
          ['clang==1', {
            # TODO: Remove once cloud_print_service_config.cc no longer depends
            # on atlapp.h, http://crbug.com/5027
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                # atlapp.h contains a global "using namespace WTL;".
                '-Wno-header-hygiene',
                # atlgdi.h does an intentional assignment in an if conditional.
                '-Wno-parentheses',
                # atlgdi.h fails with -Wreorder enabled.
                '-Wno-reorder',
                # atlgdi.h doesn't use braces around subobject initializers.
                '-Wno-missing-braces',
              ],
            },
          }],
        ],
      },
    },
    {
      # GN version: //cloud_print/service/win:cloud_print_service_setup
      'target_name': 'cloud_print_service_setup',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_setup_exe_version.rc',
        'installer.cc',
        'installer.h',
      ],
      'includes': [
        'service_resources.gypi'
      ],
      'dependencies': [
        '<(DEPTH)/cloud_print/cloud_print_resources.gyp:cloud_print_version_resources',
        '<(DEPTH)/cloud_print/common/common.gyp:cloud_print_install_lib',
        '<(DEPTH)/cloud_print/service/service.gyp:cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
  ],
}
