# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
    'nacl/nacl_defines.gypi',
  ],
  'target_defaults': {
    'variables': {
      'nacl_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below. Only files and
      # settings relevant for building the Win64 target should be added here.
      ['nacl_target==1', {
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
        ],
        'defines': [
          '<@(nacl_defines)',
        ],
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/loader/nacl_ipc_adapter.cc',
          'nacl/loader/nacl_ipc_adapter.h',
          'nacl/loader/nacl_main.cc',
          'nacl/loader/nacl_main_platform_delegate.h',
          'nacl/loader/nacl_main_platform_delegate_linux.cc',
          'nacl/loader/nacl_main_platform_delegate_mac.mm',
          'nacl/loader/nacl_main_platform_delegate_win.cc',
          'nacl/loader/nacl_listener.cc',
          'nacl/loader/nacl_listener.h',
          'nacl/loader/nacl_validation_db.h',
          'nacl/loader/nacl_validation_query.cc',
          'nacl/loader/nacl_validation_query.h',
        ],
        # TODO(gregoryd): consider switching NaCl to use Chrome OS defines
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'include_dirs': [
              '<(DEPTH)/third_party/wtl/include',
            ],
          },],
          ['OS=="linux"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'sources': [
              '../components/nacl/common/nacl_paths.cc',
              '../components/nacl/common/nacl_paths.h',
              '../components/nacl/zygote/nacl_fork_delegate_linux.cc',
              '../components/nacl/zygote/nacl_fork_delegate_linux.h',
            ],
          },],
        ],
      }],
    ],
  },
  'conditions': [
    ['disable_nacl!=1', {
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'static_library',
          'variables': {
            'nacl_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
            '../ppapi/native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
            '../ppapi/ppapi_internal.gyp:ppapi_shared',
            '../ppapi/ppapi_internal.gyp:ppapi_ipc',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'dependencies': [
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:pnacl_irt_shim',
                '../ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.gyp:pnacl_support_extension',
              ],
            }],
          ],
          'direct_dependent_settings': {
            'defines': [
              '<@(nacl_defines)',
            ],
          },
        },
      ],
      'conditions': [
        ['OS=="win" and target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'static_library',
              'variables': {
                'nacl_target': 1,
              },
              'dependencies': [
                '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome64',
                '../ppapi/ppapi_internal.gyp:ppapi_shared_win64',
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
                '../components/nacl_common.gyp:nacl_common_win64',
              ],
              'export_dependent_settings': [
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
              ],
              'sources': [
                '../components/nacl/broker/nacl_broker_listener.cc',
                '../components/nacl/broker/nacl_broker_listener.h',
                '../components/nacl/common/nacl_debug_exception_handler_win.cc',
              ],
              'include_dirs': [
                '..',
              ],
              'defines': [
                '<@(nacl_win64_defines)',
                'COMPILE_CONTENT_STATICALLY',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
              'direct_dependent_settings': {
                'defines': [
                  '<@(nacl_defines)',
                ],
              },
            },
          ],
        }],
      ],
    }, {  # else (disable_nacl==1)
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'none',
          'sources': [],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'none',
              'sources': [],
            },
          ],
        }],
      ],
    }],
  ],
}
