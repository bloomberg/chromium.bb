# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include': [
    '../native_client/build/untrusted.gypi',
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
          'NACL_BLOCK_SHIFT=5',
          'NACL_BLOCK_SIZE=32',
          '<@(nacl_defines)',
        ],
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/nacl_main.cc',
          'nacl/nacl_main_platform_delegate.h',
          'nacl/nacl_main_platform_delegate_linux.cc',
          'nacl/nacl_main_platform_delegate_mac.mm',
          'nacl/nacl_main_platform_delegate_win.cc',
          'nacl/nacl_listener.cc',
          'nacl/nacl_listener.h',
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
              'app/nacl_fork_delegate_linux.cc',
              'app/nacl_fork_delegate_linux.h',
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
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'common',
            '../ppapi/native_client/native_client.gyp:nacl_irt',
            '../webkit/support/webkit_support.gyp:glue',
            '../ppapi/native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
            '../native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
          ],
          'direct_dependent_settings': {
            'defines': [
              'NACL_BLOCK_SHIFT=5',
              'NACL_BLOCK_SIZE=32',
              '<@(nacl_defines)',
            ],
          },
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'static_library',
              'variables': {
                'nacl_target': 1,
              },
              'dependencies': [
                '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel64',
                '../native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib64',
              ],
              'sources': [
                'common/nacl_cmd_line.cc',
                'common/nacl_messages.cc',
                'nacl/nacl_broker_listener.cc',
                'nacl/nacl_broker_listener.h',
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
                  'NACL_BLOCK_SHIFT=5',
                  'NACL_BLOCK_SIZE=32',
                  '<@(nacl_defines)',
                ],
              },
            },
          ],
        }],
        ['OS=="linux" and coverage==0', {
          'conditions': [
            ['target_arch=="x64"', {
              'variables': {
                # No extra reservation.
                'nacl_reserve_top': [],
              }
            }],
            ['target_arch=="ia32"', {
              'variables': {
                # 1G address space.
                'nacl_reserve_top': ['--defsym', 'RESERVE_TOP=0x40000000'],
              }
            }],
            ['target_arch=="arm"', {
              'variables': {
                # 1G address space, plus 4K guard area above because
                # immediate offsets are 12 bits.
                'nacl_reserve_top': ['--defsym', 'RESERVE_TOP=0x40001000'],
              }
            }],
          ],
          'targets': [
            {
              'target_name': 'nacl_helper',
              'type': 'executable',
              'include_dirs': [
                '..',
              ],
              'dependencies': [
                'nacl',
              ],
              'sources': [
                'nacl/nacl_helper_linux.cc',
              ],
              'conditions': [
                ['toolkit_uses_gtk == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:gtk',
                  ],
                }],
                ['use_glib == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:glib',
                  ],
                }],
                ['os_posix == 1 and OS != "mac"', {
                  'conditions': [
                    ['linux_use_tcmalloc==1', {
                      'dependencies': [
                        '../base/allocator/allocator.gyp:allocator',
                      ],
                    }],
                  ],
                }],
              ],
              'cflags': ['-fPIE'],
              'link_settings': {
                'ldflags': ['-pie'],
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
