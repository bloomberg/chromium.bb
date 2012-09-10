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
        '../src/shared/gio/gio.gyp:gio_lib',
        '../src/shared/platform/platform.gyp:platform_lib',
        '../src/shared/srpc/srpc.gyp:srpc_lib',
        '../src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
        '../src/untrusted/irt/irt.gyp:irt_core_nexe',
        '../src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_dynacode_lib',
        '../src/untrusted/nacl/nacl.gyp:nacl_lib',
        '../src/untrusted/nosys/nosys.gyp:nosys_lib',
        '../src/untrusted/pthread/pthread.gyp:pthread_lib',
      ],
      'conditions': [
        # these libraries don't currently exist on arm
        ['target_arch!="arm"', {
          'dependencies': [
            '../src/shared/imc/imc.gyp:imc_lib',
            '../src/trusted/weak_ref/weak_ref.gyp:weak_ref_lib',
            '../src/untrusted/valgrind/valgrind.gyp:dynamic_annotations_lib',
            '../src/untrusted/valgrind/valgrind.gyp:valgrind_lib',
          ],
        }],
      ],
    },
  ],
}

