# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'mojo_common_lib',
        'mojo_common_unittests',
        'mojo_public_test_support',
        'mojo_public_unittests',
        'mojo_public_perftests',
        'mojo_system',
        'mojo_system_unittests',
        'mojo_shell_lib',
        'mojo_shell',
        'mojo_utility',
        'mojo_js',
        'sample_app',
        'mojo_bindings',
        'mojom_test',
        'mojo_bindings_test',
        'native_viewport',
      ],
    },
    {
      'target_name': 'mojo_public_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_system',
      ],
      'sources': [
        'public/tests/simple_bindings_support.cc',
        'public/tests/simple_bindings_support.h',
        'public/tests/test_support.cc',
        'public/tests/test_support.h',
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
        'common/handle_watcher.cc',
        'common/handle_watcher.h',
        'common/scoped_message_pipe.cc',
        'common/scoped_message_pipe.h',
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
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        'mojo_common_lib',
        'mojo_system',
      ],
      'sources': [
        'common/handle_watcher_unittest.cc',
        'common/test/run_all_unittests.cc',
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
      'target_name': 'mojo_public_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_support',
        'mojo_system',
      ],
      'sources': [
        'public/tests/bindings_connector_unittest.cc',
        'public/tests/system_core_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
        'mojo_system',
      ],
      'sources': [
        'public/tests/system_core_perftest.cc',
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
        'system/platform_channel_handle.h',
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
      'target_name': 'mojo_shell_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'mojo_system',
        'mojo_utility',
        'native_viewport',
      ],
      'sources': [
        'shell/app_container.cc',
        'shell/app_container.h',
        'shell/context.cc',
        'shell/context.h',
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
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'dependencies': [
        'mojo_system'
      ],
      'sources': [
        'public/utility/scoped_handle.cc',
        'public/utility/scoped_handle.h',
      ],
    },
    {
      'target_name': 'mojo_js',
      'type': 'shared_library',
      'include_dirs': [
        '..'
      ],
      'dependencies': [
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'sources': [
        'apps/js/main.cc',
        'apps/js/v8_environment.cc',
        'apps/js/v8_environment.h',
      ],
    },
    {
      'target_name': 'sample_app',
      'type': 'shared_library',
      'dependencies': [
        '../ui/gl/gl.gyp:gl',
        'mojo_system',
      ],
      'sources': [
        'examples/sample_app/sample_app.cc',
        'examples/sample_app/spinning_cube.cc',
        'examples/sample_app/spinning_cube.h',
      ],
    },
    {
      'target_name': 'mojo_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/bindings/lib/bindings.h',
        'public/bindings/lib/bindings_internal.h',
        'public/bindings/lib/bindings_serialization.cc',
        'public/bindings/lib/bindings_serialization.h',
        'public/bindings/lib/bindings_support.cc',
        'public/bindings/lib/bindings_support.h',
        'public/bindings/lib/buffer.cc',
        'public/bindings/lib/buffer.h',
        'public/bindings/lib/connector.cc',
        'public/bindings/lib/connector.h',
        'public/bindings/lib/message.cc',
        'public/bindings/lib/message.h',
        'public/bindings/lib/message_builder.cc',
        'public/bindings/lib/message_builder.h',
        'public/bindings/lib/message_queue.cc',
        'public/bindings/lib/message_queue.h',
      ],
    },
    {
      'target_name': 'mojom_test',
      'type': 'executable',
      'sources': [
        'public/bindings/sample/sample_test.cc',
        'public/bindings/sample/sample_service.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'mojo_bindings_test',
      'type': 'executable',
      'include_dirs': [
        '..',
        '<(DEPTH)/mojo/public/bindings/sample',
      ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
      'sources': [
        'public/bindings/sample/mojom/sample_service.cc',
        'public/bindings/sample/mojom/sample_service.h',
        'public/bindings/sample/mojom/sample_service_internal.h',
        'public/bindings/sample/sample_test.cc',
      ],
    },
    {
      'target_name': 'native_viewport',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:command_buffer_service',
        '../gpu/gpu.gyp:gles2_implementation',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gl/gl.gyp:gl',
      ],
      'sources': [
        'services/native_viewport/android/mojo_viewport.cc',
        'services/native_viewport/android/mojo_viewport.h',
        'services/native_viewport/native_viewport.h',
        'services/native_viewport/native_viewport_android.cc',
        'services/native_viewport/native_viewport_controller.cc',
        'services/native_viewport/native_viewport_controller.h',
        'services/native_viewport/native_viewport_mac.mm',
        'services/native_viewport/native_viewport_stub.cc',
        'services/native_viewport/native_viewport_win.cc',
        'services/native_viewport/native_viewport_x11.cc',
      ],
      'conditions': [
        ['OS=="win" or OS=="android" or OS=="linux" or OS=="mac"', {
          'sources!': [
            'services/native_viewport/native_viewport_stub.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'mojo_jni_headers',
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
