# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'nacl_ppapi_sdk',
      'type': 'none',
      'dependencies': [
        '../../../native_client/build/nacl_core_sdk.gyp:nacl_core_sdk',
        '../../../ppapi/native_client/native_client.gyp:ppapi_lib',
        '../../../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:pnacl_irt_shim',
        '../../../ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        '../../../ppapi/ppapi_untrusted.gyp:ppapi_gles2_lib',
      ],
      'conditions': [
        ['target_arch!="arm"', {
          'copies': [
            # Newlib headers
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/include',
              'files': [
                '../../../native_client/src/untrusted/irt/irt.h',
                '../../../native_client/src/untrusted/irt/irt_ppapi.h',
                '../../../native_client/src/untrusted/pthread/pthread.h',
                '../../../native_client/src/untrusted/pthread/semaphore.h',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/include/nacl',
              'files': [
                '../../../native_client/src/untrusted/nacl/nacl_dyncode.h',
                '../../../native_client/src/untrusted/nacl/nacl_startup.h',
                '../../../native_client/src/untrusted/nacl/nacl_thread.h',
              ],
            },
            # Glibc headers
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/include',
              'files': [
                '../../../native_client/src/untrusted/irt/irt.h',
                '../../../native_client/src/untrusted/irt/irt_ppapi.h',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/include/nacl',
              'files': [
                '../../../native_client/src/untrusted/nacl/nacl_dyncode.h',
                '../../../native_client/src/untrusted/nacl/nacl_startup.h',
                '../../../native_client/src/untrusted/nacl/nacl_thread.h',
              ],
            },
            # PNaCl Newlib headers
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/newlib/sdk/include',
              'files': [
                '../../../native_client/src/untrusted/irt/irt.h',
                '../../../native_client/src/untrusted/irt/irt_ppapi.h',
                '../../../native_client/src/untrusted/nacl/pnacl.h',
                '../../../native_client/src/untrusted/pthread/pthread.h',
                '../../../native_client/src/untrusted/pthread/semaphore.h',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/newlib/sdk/include/nacl',
              'files': [
                '../../../native_client/src/untrusted/nacl/nacl_dyncode.h',
                '../../../native_client/src/untrusted/nacl/nacl_startup.h',
                '../../../native_client/src/untrusted/nacl/nacl_thread.h',
              ],
            },
            # Newlib libs
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_cpp.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_gles2.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_cpp.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_gles2.a',
              ],
            },
            # Glibc libs
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/lib32',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_cpp.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_gles2.a',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/x86_64-nacl/lib',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_cpp.a',
                '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_gles2.a',
              ],
            },
            # PNaCl IRT shim
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/lib-x86-64',
              'files': [
                '<(PRODUCT_DIR)/libpnacl_irt_shim.a',
              ],
            },
          ],
        }],
        ['target_arch=="arm"', {
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/lib-arm',
              'files': [
                '<(PRODUCT_DIR)/libpnacl_irt_shim.a',
              ],
            },
          ],
        }],
      ],
    },
  ],
}

