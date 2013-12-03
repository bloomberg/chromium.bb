# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'mojo_apps.gypi',
    'mojo_examples.gypi',
    'mojo_public.gypi',
    'mojo_services.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'hello_world_service_impl',
        'mojo_bindings',
        'mojo_bindings_unittests',
        'mojo_common_lib',
        'mojo_common_unittests',
        'mojo_js',
        'mojo_js_unittests',
        'mojo_public_perftests',
        'mojo_public_test_support',
        'mojo_public_unittests',
        'mojo_shell',
        'mojo_shell_lib',
        'mojo_system',
        'mojo_system_unittests',
        'sample_app',
      ],
    },
    {
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system',
      ],
      'sources': [
        'common/test/multiprocess_test_base.cc',
        'common/test/multiprocess_test_base.h',
        'common/test/run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        'mojo_system',
      ],
      'sources': [
        'common/test/run_all_perftests.cc',
      ],
    },
    {
      'target_name': 'mojo_system',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
      'sources': [
        'system/channel.cc',
        'system/channel.h',
        'system/core.cc',
        'system/core_impl.cc',
        'system/core_impl.h',
        'system/dispatcher.cc',
        'system/dispatcher.h',
        'system/limits.h',
        'system/local_message_pipe_endpoint.cc',
        'system/local_message_pipe_endpoint.h',
        'system/memory.cc',
        'system/memory.h',
        'system/message_in_transit.cc',
        'system/message_in_transit.h',
        'system/message_pipe.cc',
        'system/message_pipe.h',
        'system/message_pipe_dispatcher.cc',
        'system/message_pipe_dispatcher.h',
        'system/message_pipe_endpoint.cc',
        'system/message_pipe_endpoint.h',
        'system/platform_channel.cc',
        'system/platform_channel.h',
        'system/platform_channel_handle.cc',
        'system/platform_channel_handle.h',
        'system/platform_channel_posix.cc',
        'system/proxy_message_pipe_endpoint.cc',
        'system/proxy_message_pipe_endpoint.h',
        'system/raw_channel.h',
        'system/raw_channel_posix.cc',
        'system/raw_channel_win.cc',
        'system/simple_dispatcher.cc',
        'system/simple_dispatcher.h',
        'system/waiter.cc',
        'system/waiter.h',
        'system/waiter_list.cc',
        'system/waiter_list.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'mojo_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        'mojo_system',
      ],
      'sources': [
        'system/core_impl_unittest.cc',
        'system/core_test_base.cc',
        'system/core_test_base.h',
        'system/dispatcher_unittest.cc',
        'system/message_pipe_dispatcher_unittest.cc',
        'system/message_pipe_unittest.cc',
        'system/raw_channel_posix_unittest.cc',
        'system/remote_message_pipe_posix_unittest.cc',
        'system/simple_dispatcher_unittest.cc',
        'system/test_utils.cc',
        'system/test_utils.h',
        'system/waiter_list_unittest.cc',
        'system/waiter_test_utils.cc',
        'system/waiter_test_utils.h',
        'system/waiter_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_common_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'mojo_system',
      ],
      'sources': [
        'common/bindings_support_impl.cc',
        'common/bindings_support_impl.h',
        'common/common_similarity_traits.cc',
        'common/common_similarity_traits.h',
        'common/handle_watcher.cc',
        'common/handle_watcher.h',
        'common/message_pump_mojo.cc',
        'common/message_pump_mojo.h',
        'common/message_pump_mojo_handler.h',
      ],
      'conditions': [
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_common_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_message_loop_tests',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_common_lib',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_system',
      ],
      'sources': [
        'common/common_similarity_traits_unittest.cc',
        'common/handle_watcher_unittest.cc',
        'common/message_pump_mojo_unittest.cc',
        'common/test/multiprocess_test_base_unittest.cc',
      ],
      'conditions': [
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_shell_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'mojo_bindings',
        'mojo_system',
        'native_viewport_impl',
      ],
      'sources': [
        'shell/app_container.cc',
        'shell/app_container.h',
        'shell/context.cc',
        'shell/context.h',
        'shell/init.cc',
        'shell/init.h',
        'shell/loader.cc',
        'shell/loader.h',
        'shell/network_delegate.cc',
        'shell/network_delegate.h',
        'shell/run.cc',
        'shell/run.h',
        'shell/storage.cc',
        'shell/storage.h',
        'shell/switches.cc',
        'shell/switches.h',
        'shell/task_runners.cc',
        'shell/task_runners.h',
        'shell/url_request_context_getter.cc',
        'shell/url_request_context_getter.h',
      ],
      'conditions': [
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_shell',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/gl/gl.gyp:gl',
        '../url/url.gyp:url_lib',
        'mojo_common_lib',
        'mojo_shell_lib',
        'mojo_system',
      ],
      'sources': [
        'shell/desktop/mojo_main.cc',
      ],
      'conditions': [
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'native_viewport_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
          ],
          'variables': {
            'java_in_dir': '<(DEPTH)/mojo/services/native_viewport/android',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'java_set_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'mojo',
            'input_java_class': 'java/util/HashSet.class',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'mojo_jni_headers',
          'type': 'none',
          'dependencies': [
            'java_set_jni_headers',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/mojo',
            ],
          },
          'sources': [
            'services/native_viewport/android/src/org/chromium/mojo/MojoViewport.java',
            'shell/android/apk/src/org/chromium/mojo_shell_apk/MojoMain.java',
          ],
          'variables': {
            'jni_gen_package': 'mojo'
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libmojo_shell',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gl/gl.gyp:gl',
            'mojo_common_lib',
            'mojo_jni_headers',
            'mojo_shell_lib',
          ],
          'sources': [
            'shell/android/library_loader.cc',
            'shell/android/mojo_main.cc',
            'shell/android/mojo_main.h',
          ],
        },
        {
          'target_name': 'mojo_shell_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../net/net.gyp:net_java',
            'native_viewport_java',
            'libmojo_shell',
          ],
          'variables': {
            'apk_name': 'MojoShell',
            'java_in_dir': '<(DEPTH)/mojo/shell/android/apk',
            'resource_dir': '<(DEPTH)/mojo/shell/android/apk/res',
            'native_lib_target': 'libmojo_shell',
          },
          'includes': [ '../build/java_apk.gypi' ],
        }
      ],
    }],
  ],
}
