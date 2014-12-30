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
          'target_name': 'monacl_syscall',
          'type': 'static_library',
          'defines': [
            '<@(nacl_defines)',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            '<(monacl_codegen_dir)/mojo_syscall.cc',
          ],
        },
        {
          'target_name': 'monacl_syscall_win64',
          'type': 'static_library',
          'defines': [
            '<@(nacl_defines)',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            '<(monacl_codegen_dir)/mojo_syscall.cc',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            }
          },
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
            'nacl/monacl_sel_main.cc',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
            'monacl_codegen',
            'monacl_syscall',
          ],
        },
        {
          'target_name': 'monacl_shell',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'mojo_edk.gyp:mojo_system_impl',
            'monacl_sel',
          ],
          'sources': [
            'nacl/monacl_shell.cc',
          ],
        },
      ],
    }],
  ],
}
