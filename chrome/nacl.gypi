# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['disable_nacl!=1', {
      'conditions': [
        ['OS=="linux"', {
          'includes': [
            '../components/nacl/nacl_defines.gypi',
          ],
          'targets': [
            {
              'target_name': 'nacl_helper',
              'type': 'executable',
              'include_dirs': [
                '..',
              ],
              'dependencies': [
                '../components/nacl.gyp:nacl',
                '../components/nacl_common.gyp:nacl_common',
                '../crypto/crypto.gyp:crypto',
                '../sandbox/sandbox.gyp:libc_urandom_override',
                '../sandbox/sandbox.gyp:sandbox',
              ],
              'defines': [
                '<@(nacl_defines)',
              ],
              'sources': [
                'nacl/nacl_helper_linux.cc',
                '../base/posix/unix_domain_socket_linux.cc',
                '../components/nacl/loader/nacl_sandbox_linux.cc',
                '../content/common/child_process_sandbox_support_impl_shm_linux.cc',
                '../content/common/sandbox_init_linux.cc',
                '../content/common/sandbox_seccomp_bpf_linux.cc',
                '../content/public/common/content_switches.cc',
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
          ],
        }],
      ],
    }],
  ],
}
