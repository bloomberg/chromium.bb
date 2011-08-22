# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
              'nacl/nacl_fork_delegate_linux.cc',
            ],
          },],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'nacl',
      'type': 'static_library',
      'variables': {
        'nacl_target': 1,
        'irt_build_cmd': [
          'python', 'build_nacl_irt.py', '--outdir', '<(PRODUCT_DIR)',
        ],
        'irt_inputs_cmd':
            'python build_nacl_irt.py --inputs',
      },
      'dependencies': [
        # TODO(gregoryd): chrome_resources and chrome_strings could be
        # shared with the 64-bit target, but it does not work due to a gyp
        #issue
        'chrome_resources',
        'chrome_strings',
        'common',
        '../webkit/support/webkit_support.gyp:glue',
        '../native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
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
      'conditions': [
        ['OS=="win"', {
          # Windows needs both the x86-32 and x86-64 IRT.
          'actions': [
            {
              'action_name': 'nacl_irt',
              'message': 'Building NaCl IRT',
              'inputs': [
                '<!@(<(irt_inputs_cmd) --platform=x86-32 --platform=x86-64)',
              ],
              'outputs': ['<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
                          '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe'],
              'action': [
                '<@(irt_build_cmd)',
                '--platform', 'x86-32',
                '--platform', 'x86-64',
              ],
            },
          ],
        }],
        ['OS!="win" and target_arch=="ia32"', {
          # Linux-x86-32 and OSX need only the x86-32 IRT.
          'actions': [
            {
              'action_name': 'nacl_irt',
              'message': 'Building NaCl IRT',
              'inputs': [
                '<!@(<(irt_inputs_cmd) --platform=x86-32)',
              ],
              'outputs': ['<(PRODUCT_DIR)/nacl_irt_x86_32.nexe'],
              'action': [
                '<@(irt_build_cmd)', '--platform', 'x86-32',
              ],
            },
          ],
        }],
        ['OS!="win" and target_arch=="x64"', {
          # Linux-x86-64 needs only the x86-64 IRT.
          'actions': [
            {
              'action_name': 'nacl_irt',
              'message': 'Building NaCl IRT',
              'inputs': [
                '<!@(<(irt_inputs_cmd) --platform=x86-64)',
              ],
              'outputs': ['<(PRODUCT_DIR)/nacl_irt_x86_64.nexe'],
              'action': [
                '<@(irt_build_cmd)', '--platform', 'x86-64',
              ],
            },
          ],
        }],
      ],
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
            # TODO(gregoryd): chrome_resources and chrome_strings could be
            # shared with the 32-bit target, but it does not work due to a gyp
            #issue
            'chrome_resources',
            'chrome_strings',
            'common_nacl_win64',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel64',
            '../native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib64',
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
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'nacl_helper.so',
          # 'executable' will be overridden below when we add the -shared
          # flag; here it prevents gyp from using the --whole-archive flag
          'type': 'executable',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'nacl',
          ],
          'sources': [
            '../chrome/nacl/nacl_helper_linux.cc',
          ],
          'conditions': [
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
          ],
          'link_settings': {
            # NOTE: '-shared' overrides 'executable' above
            'ldflags': ['-shared',
                        '-Wl,--version-script=chrome/nacl/nacl_helper_exports.txt',
                      ],
          },
        },
        {
          'target_name': 'nacl_helper_bootstrap',
          'type': 'executable',
          'dependencies': [
            'nacl_helper.so',
          ],
          'sources': [
            '../chrome/nacl/nacl_helper_bootstrap_linux.c',
          ],
          # TODO(bradchen): Delete the -B argument when Gold supports
          # -Ttext properly. Until then use ld.bfd.
          'link_settings': {
            'ldflags': ['-B', 'tools/ld_bfd',
                        # Force text segment at 0x10000 (64KB)
                        # The max-page-size option is needed on x86-64 linux
                        # where 4K pages are not the default in the BFD linker.
                        '-Wl,-Ttext-segment,10000,-z,max-page-size=0x1000',
                        # reference nacl_helper as a shared library
                        '<(PRODUCT_DIR)/nacl_helper.so',
                        '-Wl,-rpath,<(SHARED_LIB_DIR)',
                      ],
          },
        },
      ],
    }],
  ],
}
