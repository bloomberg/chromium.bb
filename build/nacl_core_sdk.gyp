# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
  'variables': {
    'disable_cross_trusted%': 0,
  },
  'targets': [
    {
      'target_name': 'nacl_core_sdk',
      'type': 'none',
      'dependencies': [
        '../src/untrusted/minidump_generator/minidump_generator.gyp:minidump_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_dynacode_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_exception_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_list_mappings_lib',
        '../src/untrusted/nosys/nosys.gyp:nosys_lib',
        '../src/untrusted/pthread/pthread.gyp:pthread_lib',
      ],
      'conditions': [
        ['(target_arch!="arm" and target_arch!="mipsel") or (OS=="linux" and disable_cross_trusted!=1)', {
          # cross compiling trusted binaries such as sel_ldr is only currently
          # supported on linux, and requires specific cross compilers to be
          # installed. It can be disabled with 'disable_cross_trusted=1'.
          'dependencies': [
            '../src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
          ],
        }],
        ['target_arch=="ia32" and OS=="linux"', {
          'dependencies': [
            '../src/nonsfi/loader/loader.gyp:nonsfi_loader',
          ],
        }],
        ['(target_arch!="arm" and target_arch!="mipsel") or OS=="linux"', {
          # irt_core_nexe relies on tls_edit which is not currently buildable
          # on windows with target_arch != x86.
          # TODO(sbc): remove this restriction:
          # https://code.google.com/p/nativeclient/issues/detail?id=3810
          'dependencies': [
            '../src/untrusted/irt/irt.gyp:irt_core_nexe',
          ],
        }],
        ['target_arch!="arm" and target_arch!="mipsel"', {
          'dependencies': [
            # these libraries don't currently exist on arm and mips
            '../src/untrusted/valgrind/valgrind.gyp:dynamic_annotations_lib',
            '../src/untrusted/valgrind/valgrind.gyp:valgrind_lib',
          ],
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(OS)_x86/nacl_x86_newlib/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crti.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crtn.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libnacl_list_mappings.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libnosys.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libpthread.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libvalgrind.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(OS)_x86/nacl_x86_newlib/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crti.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crtn.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libnacl_list_mappings.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libnosys.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libpthread.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libvalgrind.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(OS)_x86/nacl_x86_glibc/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libnacl_list_mappings.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libvalgrind.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libdynamic_annotations.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/<(OS)_x86/nacl_x86_glibc/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libnacl_list_mappings.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libvalgrind.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libdynamic_annotations.a',
              ],
            },
          ],
        }],
      ],
    },
  ],
}

