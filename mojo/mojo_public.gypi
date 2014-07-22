# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //mojo/public/c/system
      'target_name': 'mojo_system',
      'type': 'static_library',
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'all_dependent_settings': {
        'conditions': [
          # We need to be able to call the MojoSetSystemThunks() function in
          # system_thunks.cc
          ['OS=="android"', {
            'ldflags!': [
              '-Wl,--exclude-libs=ALL',
            ],
          }],
        ],
      },
      'sources': [
        'public/c/system/buffer.h',
        'public/c/system/core.h',
        'public/c/system/data_pipe.h',
        'public/c/system/functions.h',
        'public/c/system/macros.h',
        'public/c/system/message_pipe.h',
        'public/c/system/system_export.h',
        'public/c/system/types.h',
        'public/platform/native/system_thunks.cc',
        'public/platform/native/system_thunks.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings
      'target_name': 'mojo_cpp_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/cpp/bindings/array.h',
        'public/cpp/bindings/callback.h',
        'public/cpp/bindings/error_handler.h',
        'public/cpp/bindings/interface_impl.h',
        'public/cpp/bindings/interface_ptr.h',
        'public/cpp/bindings/interface_request.h',
        'public/cpp/bindings/message.h',
        'public/cpp/bindings/message_filter.h',
        'public/cpp/bindings/no_interface.h',
        'public/cpp/bindings/string.h',
        'public/cpp/bindings/type_converter.h',
        'public/cpp/bindings/lib/array_internal.h',
        'public/cpp/bindings/lib/array_internal.cc',
        'public/cpp/bindings/lib/array_serialization.h',
        'public/cpp/bindings/lib/bindings_internal.h',
        'public/cpp/bindings/lib/bindings_serialization.cc',
        'public/cpp/bindings/lib/bindings_serialization.h',
        'public/cpp/bindings/lib/bounds_checker.cc',
        'public/cpp/bindings/lib/bounds_checker.h',
        'public/cpp/bindings/lib/buffer.h',
        'public/cpp/bindings/lib/callback_internal.h',
        'public/cpp/bindings/lib/connector.cc',
        'public/cpp/bindings/lib/connector.h',
        'public/cpp/bindings/lib/filter_chain.cc',
        'public/cpp/bindings/lib/filter_chain.h',
        'public/cpp/bindings/lib/fixed_buffer.cc',
        'public/cpp/bindings/lib/fixed_buffer.h',
        'public/cpp/bindings/lib/interface_impl_internal.h',
        'public/cpp/bindings/lib/interface_ptr_internal.h',
        'public/cpp/bindings/lib/message.cc',
        'public/cpp/bindings/lib/message_builder.cc',
        'public/cpp/bindings/lib/message_builder.h',
        'public/cpp/bindings/lib/message_filter.cc',
        'public/cpp/bindings/lib/message_header_validator.cc',
        'public/cpp/bindings/lib/message_header_validator.h',
        'public/cpp/bindings/lib/message_internal.h',
        'public/cpp/bindings/lib/message_queue.cc',
        'public/cpp/bindings/lib/message_queue.h',
        'public/cpp/bindings/lib/no_interface.cc',
        'public/cpp/bindings/lib/router.cc',
        'public/cpp/bindings/lib/router.h',
        'public/cpp/bindings/lib/shared_data.h',
        'public/cpp/bindings/lib/shared_ptr.h',
        'public/cpp/bindings/lib/string_serialization.h',
        'public/cpp/bindings/lib/string_serialization.cc',
        'public/cpp/bindings/lib/validation_errors.cc',
        'public/cpp/bindings/lib/validation_errors.h',
      ],
    },
    {
      # GN version: //mojo/public/js/bindings
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/js/bindings/constants.cc',
        'public/js/bindings/constants.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment:standalone
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/c/environment/async_waiter.h',
        'public/c/environment/logger.h',
        'public/cpp/environment/environment.h',
        'public/cpp/environment/lib/default_async_waiter.cc',
        'public/cpp/environment/lib/default_async_waiter.h',
        'public/cpp/environment/lib/default_logger.cc',
        'public/cpp/environment/lib/default_logger.h',
        'public/cpp/environment/lib/environment.cc',
        'public/cpp/environment/lib/logging.cc',
        'public/cpp/environment/logging.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # GN version: //mojo/public/cpp/utility
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/cpp/utility/mutex.h',
        'public/cpp/utility/run_loop.h',
        'public/cpp/utility/run_loop_handler.h',
        'public/cpp/utility/thread.h',
        'public/cpp/utility/lib/mutex.cc',
        'public/cpp/utility/lib/run_loop.cc',
        'public/cpp/utility/lib/thread.cc',
        'public/cpp/utility/lib/thread_local.h',
        'public/cpp/utility/lib/thread_local_posix.cc',
        'public/cpp/utility/lib/thread_local_win.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/mutex.h',
            'public/cpp/utility/thread.h',
            'public/cpp/utility/lib/mutex.cc',
            'public/cpp/utility/lib/thread.cc',
          ],
        }],
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/service_provider:service_provider
      'target_name': 'mojo_service_provider_bindings',
      'type': 'static_library',
      'sources': [
        'public/interfaces/service_provider/service_provider.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application
      'target_name': 'mojo_application',
      'type': 'static_library',
      'sources': [
        'public/cpp/application/application_impl.h',
        'public/cpp/application/connect.h',
        'public/cpp/application/lib/application_impl.cc',
        'public/cpp/application/lib/service_connector.cc',
        'public/cpp/application/lib/service_connector.h',
        'public/cpp/application/lib/application_connection.cc',
        'public/cpp/application/lib/application_delegate.cc',
        'public/cpp/application/lib/service_registry.cc',
        'public/cpp/application/lib/service_registry.h',
      ],
      'dependencies': [
        'mojo_service_provider_bindings',
      ],
      'export_dependent_settings': [
        'mojo_service_provider_bindings',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //mojo/public/java_system
          'target_name': 'mojo_public_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'public/java/system',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //mojo/public/java_bindings
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'public/java/bindings',
          },
          'dependencies': [
            'mojo_public_java',
          ],
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
  ],
}
