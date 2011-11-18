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
              'target_name': 'nacl_helper_bootstrap_lib',
              'type': 'static_library',
              'product_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
              'hard_depencency': 1,
              'include_dirs': [
                '..',
              ],
              'sources': [
                'nacl/nacl_helper_bootstrap_linux.c',
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
              'cflags!': [
                '-fasan',
                '-w',
              ],
              'conditions': [
                ['clang==1', {
                  'cflags': [
                    # Prevent llvm-opt from replacing my_bzero with a call
                    # to memset
                    '-ffreestanding',
                    # But make its <limits.h> still work!
                    '-U__STDC_HOSTED__', '-D__STDC_HOSTED__=1',
                  ],
                }],
              ],
            },
            {
              'target_name': 'nacl_helper_bootstrap_raw',
              'type': 'none',
              'dependencies': [
                'nacl_helper_bootstrap_lib',
              ],
              'actions': [
                {
                  'action_name': 'link_with_ld_bfd',
                  'variables': {
                    'bootstrap_lib': '<(SHARED_INTERMEDIATE_DIR)/chrome/<(STATIC_LIB_PREFIX)nacl_helper_bootstrap_lib<(STATIC_LIB_SUFFIX)',
                    'linker_script': 'nacl/nacl_helper_bootstrap_linux.x',
                  },
                  'inputs': [
                     '<(linker_script)',
                     '<(bootstrap_lib)',
                     '../tools/ld_bfd/ld',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/nacl_helper_bootstrap_raw',
                  ],
                  'message': 'Linking nacl_helper_bootstrap_raw',
                  'conditions': [
                    ['target_arch=="x64"', {
                      'variables': {
                        'linker_emulation': 'elf_x86_64',
                        'bootstrap_extra_lib': '',
                      }
                    }],
                    ['target_arch=="ia32"', {
                      'variables': {
                        'linker_emulation': 'elf_i386',
                        'bootstrap_extra_lib': '',
                      }
                    }],
                    ['target_arch=="arm"', {
                      'variables': {
                        'linker_emulation': 'armelf_linux_eabi',
                        # ARM requires linking against libc due to ABI
                        # dependencies on memset.
                        'bootstrap_extra_lib' : "${SYSROOT}/usr/lib/libc.a",
                      }
                    }],
                  ],
                  'action': ['../tools/ld_bfd/ld',
                             '-m', '<(linker_emulation)',
                             '--build-id',
                             # This program is (almost) entirely
                             # standalone.  It has its own startup code, so
                             # no crt1.o for it.  It is statically linked,
                             # and on x86 it does not use libc at all.
                             # However, on ARM it needs a few (safe) things
                             # from libc.
                             '-static',
                             # Link with custom linker script for special
                             # layout.  The script uses the symbol RESERVE_TOP.
                             '<@(nacl_reserve_top)',
                             '--script=<(linker_script)',
                             '-o', '<@(_outputs)',
                             # On x86-64, the default page size with some
                             # linkers is 2M rather than the real Linux page
                             # size of 4K.  A larger page size is incompatible
                             # with our custom linker script's special layout.
                             '-z', 'max-page-size=0x1000',
                             '--whole-archive', '<(bootstrap_lib)',
                             '--no-whole-archive',
                             '<@(bootstrap_extra_lib)',
                           ],
                }
              ],
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
