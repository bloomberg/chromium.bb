# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'variables': {
        'monacl_codegen_dir': '<(SHARED_INTERMEDIATE_DIR)/<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))/monacl',
      },
      'includes': [
        'mojo_variables.gypi',
        '../build/common_untrusted.gypi',
        '../components/nacl/nacl_defines.gypi',
      ],
      'targets': [
        {
          'target_name': 'libmojo',
          'type': 'none',
          'variables': {
            'nlib_target': 'libmojo.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_pnacl_newlib': 1,
          },
          'defines': [
            '<@(nacl_defines)',
          ],
          'sources': [
            '<(monacl_codegen_dir)/libmojo.cc',
          ],
          'dependencies': [
            'mojo_nacl.gyp:monacl_codegen',
          ],
        },
        {
          'target_name': 'monacl_test',
          'type': 'none',
          'variables': {
            'nexe_target': 'monacl_test',
            'build_newlib': 0,
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'link_flags': [
              '-pthread',
              '-lmojo',
              '-limc_syscalls',
            ],
            'sources': [
              '<@(mojo_public_system_unittest_sources)',
            ],
          },
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
            '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
            '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
            '<(DEPTH)/native_client/src/untrusted/pthread/pthread.gyp:pthread_lib',
            '../testing/gtest_nacl.gyp:gtest_nacl',
            '../testing/gtest_nacl.gyp:gtest_main_nacl',
            'libmojo',
            'mojo_nacl.gyp:monacl_codegen',
          ],
        },
      ],
    }],
  ],
}
