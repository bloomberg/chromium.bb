# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
  ],
  'targets': [
    {
      'target_name': 'ipc_mojo',
      'type': '<(component)',
      'variables': {
      },
      'defines': [
        'IPC_MOJO_IMPLEMENTATION',
      ],
      'dependencies': [
        '../ipc.gyp:ipc',
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/mojo_base.gyp:mojo_system_impl',
      ],
      'sources': [
        'ipc_channel_mojo.cc',
        'ipc_channel_mojo.h',
        'ipc_channel_mojo_host.cc',
        'ipc_channel_mojo_host.h',
        'ipc_channel_mojo_readers.cc',
        'ipc_channel_mojo_readers.h',
        'ipc_mojo_bootstrap.cc',
        'ipc_mojo_bootstrap.h',
        'ipc_message_pipe_reader.cc',
        'ipc_message_pipe_reader.h',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      # 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'ipc_mojo_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../ipc.gyp:ipc',
        '../ipc.gyp:test_support_ipc',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:test_support_base',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/mojo_base.gyp:mojo_system_impl',
        '../../testing/gtest.gyp:gtest',
        'ipc_mojo',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'run_all_unittests.cc',
        'ipc_channel_mojo_unittest.cc',
        'ipc_mojo_bootstrap_unittest.cc',
      ],
      'conditions': [
      ],
    },
    {
      'target_name': 'ipc_mojo_perftests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../ipc.gyp:ipc',
        '../ipc.gyp:test_support_ipc',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:test_support_base',
        '../../base/base.gyp:test_support_perf',
        '../../mojo/mojo_base.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/mojo_base.gyp:mojo_system_impl',
        '../../testing/gtest.gyp:gtest',
        'ipc_mojo',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'ipc_mojo_perftest.cc',
      ],
      'conditions': [
      ],
    },
  ],
}
