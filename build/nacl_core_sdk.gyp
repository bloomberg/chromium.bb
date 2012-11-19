# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_core_sdk',
      'type': 'none',
      'dependencies': [
        '../src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
        '../src/untrusted/irt/irt.gyp:irt_core_nexe',
        '../src/untrusted/nacl/nacl.gyp:nacl_dynacode_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_lib',
        '../src/untrusted/nosys/nosys.gyp:nosys_lib',
        '../src/untrusted/pthread/pthread.gyp:pthread_lib',
      ],
      'conditions': [
        # these libraries don't currently exist on arm
        ['target_arch!="arm"', {
          'dependencies': [
            '../src/untrusted/valgrind/valgrind.gyp:dynamic_annotations_lib',
            '../src/untrusted/valgrind/valgrind.gyp:valgrind_lib',
          ],
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crti.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/crtn.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libnosys.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libpthread.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libvalgrind.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crti.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/crtn.o',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libnosys.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libpthread.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libvalgrind.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libnacl_dyncode.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libvalgrind.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libdynamic_annotations.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libnacl_dyncode.a',
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

