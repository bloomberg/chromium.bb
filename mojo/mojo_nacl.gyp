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
          'target_name': 'monacl_codegen',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate_nacl_bindings',
              'inputs': [
                'nacl/generator/generate_nacl_bindings.py',
                'nacl/generator/interface.py',
                'nacl/generator/interface_dsl.py',
                'nacl/generator/mojo_syscall.cc.tmpl',
                'nacl/generator/libmojo.cc.tmpl',
              ],
              'outputs': [
                '<(monacl_codegen_dir)/mojo_syscall.cc',
                '<(monacl_codegen_dir)/libmojo.cc',
              ],
              'action': [
                'python',
                'nacl/generator/generate_nacl_bindings.py',
                '-d', '<(monacl_codegen_dir)',
              ],
            },
          ],
        },
        {
          'target_name': 'monacl_sel',
          'type': 'static_library',
          'defines': [
            '<@(nacl_defines)',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            '<(monacl_codegen_dir)/mojo_syscall.cc',
            'nacl/monacl_sel_main.cc',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
          ],
        },
        {
          'target_name': 'monacl_shell',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'mojo_base.gyp:mojo_system_impl',
            'monacl_sel',
          ],
          'sources': [
            'nacl/monacl_shell.cc',
          ],
        },
        {
          'target_name': 'mojo_nacl',
          'type': 'none',
          'variables': {
            'nlib_target': 'libmojo.a',
            'build_glibc': 0,
            'build_newlib': 1,
            'build_pnacl_newlib': 0,
          },
          'defines': [
            '<@(nacl_defines)',
          ],
          'sources': [
            '<(monacl_codegen_dir)/libmojo.cc',
          ],
          'dependencies': [
            'monacl_codegen',
          ],
        },
        {
          'target_name': 'monacl_test',
          'type': 'none',
          'variables': {
            'nexe_target': 'monacl_test',
            'build_newlib': 1,
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
            'mojo_nacl',
            'monacl_codegen',
          ],
        },
      ],
    }],
  ],
}
