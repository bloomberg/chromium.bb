# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'includes': [
        '../mojo/mojo_nacl.gypi',
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
                'nacl/generator/libmojo.cc.tmpl',
                'nacl/generator/mojo_irt.c.tmpl',
                'nacl/generator/mojo_irt.h.tmpl',
                'nacl/generator/mojo_syscall.cc.tmpl',
              ],
              'outputs': [
                '<(monacl_codegen_dir)/libmojo.cc',
                '<(monacl_codegen_dir)/mojo_irt.c',
                '<(monacl_codegen_dir)/mojo_irt.h',
                '<(monacl_codegen_dir)/mojo_syscall.cc',
              ],
              'action': [
                'python',
                'nacl/generator/generate_nacl_bindings.py',
                '-d', '<(monacl_codegen_dir)',
              ],
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [ '../third_party/mojo/src/' ],
          },
        },
        {
          'target_name': 'monacl_syscall',
          'type': 'static_library',
          'include_dirs': [
            '..',
            '../third_party/mojo/src/',
          ],
          'sources': [
            '<(monacl_codegen_dir)/mojo_syscall.cc',
          ],
          'dependencies': [
            '../third_party/mojo/mojo_public.gyp:mojo_system_placeholder',
          ],
        },
        {
          'target_name': 'monacl_sel',
          'type': 'static_library',
          'include_dirs': [
            '..',
            '../third_party/mojo/src/',
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
            '../third_party/mojo/mojo_edk.gyp:mojo_system_impl',
            'monacl_sel',
          ],
          'sources': [
            'nacl/monacl_shell.cc',
          ],
        },
      ],
      'conditions': [
        ['OS=="win" and target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'monacl_syscall_win64',
              'type': 'static_library',
              'include_dirs': [
                '..',
              ],
              'sources': [
                '<(monacl_codegen_dir)/mojo_syscall.cc',
              ],
              'dependencies': [
                '../third_party/mojo/mojo_public.gyp:mojo_system_placeholder',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                }
              },
            },
          ],
        }],
      ],
    }],
  ],
}
