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
          ],
          'cflags': ['-fPIE'],
          'link_settings': {
            'ldflags': ['-pie'],
          },
        },
        {
          'target_name': 'nacl_helper_bootstrap_munge_phdr',
          'type': 'executable',
          'toolsets': ['host'],
          'sources': [
            'nacl/nacl_helper_bootstrap_munge_phdr.c',
          ],
          'libraries': [
            '-lelf',
          ],
          # This is an ugly kludge because gyp doesn't actually treat
          # host_arch=x64 target_arch=ia32 as proper cross compilation.
          # It still wants to compile the "host" program with -m32 et
          # al.  Though a program built that way can indeed run on the
          # x86-64 host, we cannot reliably build this program on such a
          # host because Ubuntu does not provide the full suite of
          # x86-32 libraries in packages that can be installed on an
          # x86-64 host; in particular, libelf is missing.  So here we
          # use the hack of eliding all the -m* flags from the
          # compilation lines, getting the command close to what they
          # would be if gyp were to really build properly for the host.
          # TODO(bradnelson): Clean up with proper cross support.
          'conditions': [
            ['host_arch=="x64"', {
              'cflags/': [['exclude', '-m.*']],
              'ldflags/': [['exclude', '-m.*']],
            }],
          ],
        },
        {
          'target_name': 'nacl_helper_bootstrap_raw',
          'type': 'executable',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'nacl/nacl_helper_bootstrap_linux.c',
            # We list the linker script here for documentation purposes.
            # But even this doesn't make gyp treat it as a dependency,
            # so incremental builds won't relink when the script changes.
            # TODO(bradnelson): Fix the dependency handling.
            'nacl/nacl_helper_bootstrap_linux.x',
          ],
          'cflags': [
            # The tiny standalone bootstrap program is incompatible with
            # -fstack-protector, which might be on by default.  That switch
            # requires using the standard libc startup code, which we do not.
            '-fno-stack-protector',
            # We don't want to compile it PIC (or its cousin PIE), because
            # it goes at an absolute address anyway, and because any kind
            # of PIC complicates life for the x86-32 assembly code.  We
            # append -fno-* flags here instead of using a 'cflags!' stanza
            # to remove -f* flags, just in case some system's compiler
            # defaults to using PIC for everything.
            '-fno-pic', '-fno-PIC',
            '-fno-pie', '-fno-PIE',
          ],
          'link_settings': {
            'ldflags': [
              # TODO(bradchen): Delete the -B argument when Gold is verified
              # to produce good results with our custom linker script.
              # Until then use ld.bfd.
              '-B', '<(PRODUCT_DIR)/../../tools/ld_bfd',
              # This programs is (almost) entirely standalone.  It has
              # its own startup code, so no crt1.o for it.  It is
              # statically linked, and on x86 it actually does not use
              # libc at all.  However, on ARM it needs a few (safe)
              # things from libc, so we don't use '-nostdlib' here.
              '-static', '-nostartfiles',
              # Link with our custom linker script to get out special layout.
              '-Wl,--script=<(PRODUCT_DIR)/../../chrome/nacl/nacl_helper_bootstrap_linux.x',
              # On x86-64, the default page size with some
              # linkers is 2M rather than the real Linux page
              # size of 4K.  A larger page size is incompatible
              # with our custom linker script's special layout.
              '-Wl,-z,max-page-size=0x1000',
            ],
          },
        },
        {
          'target_name': 'nacl_helper_bootstrap',
          'dependencies': [
            'nacl_helper_bootstrap_raw',
            'nacl_helper_bootstrap_munge_phdr#host',
          ],
          'type': 'none',
          'actions': [{
              'action_name': 'munge_phdr',
              'inputs': ['nacl/nacl_helper_bootstrap_munge_phdr.py',
                         '<(PRODUCT_DIR)/nacl_helper_bootstrap_munge_phdr',
                         '<(PRODUCT_DIR)/nacl_helper_bootstrap_raw'],
              'outputs': ['<(PRODUCT_DIR)/nacl_helper_bootstrap'],
              'message': 'Munging ELF program header',
              'action': ['python', '<@(_inputs)', '<@(_outputs)']
            }],
          }
      ],
    }],
  ],
}
