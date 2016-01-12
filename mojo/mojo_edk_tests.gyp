# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/mojo/mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_edk_tests',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_message_pipe_perftests2',
        'mojo_system_unittests2',
        'mojo_js_unittests',
        'mojo_js_integration_tests',
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_system_unittests
      'target_name': 'mojo_system_unittests2',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support2',
        'mojo_edk.gyp:mojo_system_impl2',
      ],
      'sources': [
        'edk/embedder/embedder_unittest.cc',
        'edk/embedder/platform_channel_pair_posix_unittest.cc',
        'edk/embedder/simple_platform_shared_buffer_unittest.cc',
        'edk/system/awakable_list_unittest.cc',
        'edk/system/core_test_base.cc',
        'edk/system/core_test_base.h',
        'edk/system/core_unittest.cc',
        'edk/system/data_pipe_unittest.cc',
        'edk/system/dispatcher_unittest.cc',
        'edk/system/message_in_transit_queue_unittest.cc',
        'edk/system/message_in_transit_test_utils.cc',
        'edk/system/message_in_transit_test_utils.h',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/message_pipe_unittest.cc',
        'edk/system/multiprocess_message_pipe_unittest.cc',
        'edk/system/options_validation_unittest.cc',
        'edk/system/platform_handle_dispatcher_unittest.cc',
        'edk/system/raw_channel_unittest.cc',
        'edk/system/run_all_unittests.cc',
        'edk/system/shared_buffer_dispatcher_unittest.cc',
        'edk/system/simple_dispatcher_unittest.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
        'edk/system/wait_set_dispatcher_unittest.cc',
        'edk/system/waiter_test_utils.cc',
        'edk/system/waiter_test_utils.h',
        'edk/system/waiter_unittest.cc',
        'edk/test/multiprocess_test_helper_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/embedder/embedder_unittest.cc',
            'edk/system/multiprocess_message_pipe_unittest.cc',
            'edk/test/multiprocess_test_helper_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests2',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support2',
        'mojo_edk.gyp:mojo_system_impl2',
      ],
      'sources': [
        'edk/system/message_pipe_perftest.cc',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
      ],
    },
    # TODO(yzshen): fix the following two targets.
    {
      # GN version: //mojo/edk/js/test:js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        '../third_party/mojo/mojo_public.gyp:mojo_environment_standalone',
        '../third_party/mojo/mojo_public.gyp:mojo_public_test_interfaces',
        '../third_party/mojo/mojo_public.gyp:mojo_utility',
        'mojo_edk.gyp:mojo_common_test_support2',
        'mojo_edk.gyp:mojo_run_all_unittests2',
        'mojo_edk.gyp:mojo_js_lib2',
      ],
      'sources': [
        'edk/js/handle_unittest.cc',
        'edk/js/test/run_js_tests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/js/test:js_integration_tests
      'target_name': 'mojo_js_integration_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin_test',
        '../third_party/mojo/mojo_public.gyp:mojo_environment_standalone',
        '../third_party/mojo/mojo_public.gyp:mojo_public_test_interfaces',
        '../third_party/mojo/mojo_public.gyp:mojo_utility',
        'mojo_edk.gyp:mojo_js_lib2',
        'mojo_edk.gyp:mojo_run_all_unittests2',
        'mojo_js_to_cpp_bindings',
      ],
      'sources': [
        'edk/js/test/run_js_integration_tests.cc',
        'edk/js/tests/js_to_cpp_tests.cc',
      ],
    },
  ],
}
