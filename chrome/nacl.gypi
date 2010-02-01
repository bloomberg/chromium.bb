# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
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
          'NACL_BLOCK_SHIFT=5',
          'NACL_BLOCK_SIZE=32',
          '<@(nacl_defines)',
        ],
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/nacl_main.cc',
          'nacl/nacl_thread.cc',
          'nacl/nacl_thread.h',
          'nacl/sel_main.cc',
        ],
        # TODO(gregoryd): consider switching NaCl to use Chrome OS defines
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STD_C',
              '_CRT_SECURE_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
            ],
            'include_dirs': [
              'third_party/wtl/include',
            ],
          },],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'nacl',
      'type': '<(library)',
      'msvs_guid': '83E86DAF-5763-4711-AD34-5FDAE395560C',
      'variables': {
        'nacl_target': 1,
      },
      'dependencies': [
        # TODO(gregoryd): chrome_resources and chrome_strings could be
        # shared with the 64-bit target, but it does not work due to a gyp
        #issue
        'chrome_resources',
        'chrome_strings',
        'common',
        '../third_party/npapi/npapi.gyp:npapi',
        '../webkit/webkit.gyp:glue',
        '../native_client/src/trusted/plugin/plugin.gyp:npGoogleNaClPluginChrome',
        '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
        '../native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate',
        '../native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'nacl_win64',
          'type': '<(library)',
          'msvs_guid': '14135464-9FB9-42E3-99D8-791116FA1204',
          'variables': {
            'nacl_target': 1,
          },
          'dependencies': [
            # TODO(gregoryd): chrome_resources and chrome_strings could be
            # shared with the 32-bit target, but it does not work due to a gyp
            #issue
            'chrome_resources',
            'chrome_strings',
            'common_nacl_win64',
          ],
          'sources': [
            'nacl/broker_thread.cc',
            'nacl/broker_thread.h',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
