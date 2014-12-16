# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_public',
      'type': 'none',
      'dependencies': [
        'mojo_js_bindings',
        'mojo_public_test_interfaces',
        'mojo_public_test_utils',
        'mojo_system',
        'mojo_utility',
      ],
    },
    {
      'target_name': 'mojo_public_none',
      'type': 'none',
    },
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
        'public/cpp/bindings/binding.h',
        'public/cpp/bindings/callback.h',
        'public/cpp/bindings/error_handler.h',
        'public/cpp/bindings/interface_impl.h',
        'public/cpp/bindings/interface_ptr.h',
        'public/cpp/bindings/interface_request.h',
        'public/cpp/bindings/message.h',
        'public/cpp/bindings/message_filter.h',
        'public/cpp/bindings/no_interface.h',
        'public/cpp/bindings/string.h',
        'public/cpp/bindings/strong_binding.h',
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
        'public/cpp/bindings/lib/interface_ptr_internal.h',
        'public/cpp/bindings/lib/map_data_internal.h',
        'public/cpp/bindings/lib/map_internal.h',
        'public/cpp/bindings/lib/map_serialization.h',
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
        'public/cpp/bindings/lib/validate_params.h',
        'public/cpp/bindings/lib/validation_errors.cc',
        'public/cpp/bindings/lib/validation_errors.h',
      ],
    },
    {
      # GN version: //mojo/public/js
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/js/constants.cc',
        'public/js/constants.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment:standalone
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/c/environment/async_waiter.h',
        'public/c/environment/logger.h',
        'public/cpp/environment/async_waiter.h',
        'public/cpp/environment/environment.h',
        'public/cpp/environment/lib/async_waiter.cc',
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
      'target_name': 'mojo_application_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/application/application.mojom',
          'public/interfaces/application/service_provider.mojom',
          'public/interfaces/application/shell.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/application:application
      'target_name': 'mojo_application_bindings',
      'type': 'static_library',
      'dependencies': [
        'mojo_application_bindings_mojom',
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application
      'target_name': 'mojo_application_base',
      'type': 'static_library',
      'sources': [
        'public/cpp/application/application_connection.h',
        'public/cpp/application/application_delegate.h',
        'public/cpp/application/application_impl.h',
        'public/cpp/application/connect.h',
        'public/cpp/application/interface_factory.h',
        'public/cpp/application/interface_factory_impl.h',
        'public/cpp/application/lib/application_connection.cc',
        'public/cpp/application/lib/application_delegate.cc',
        'public/cpp/application/lib/application_impl.cc',
        'public/cpp/application/lib/service_provider_impl.cc',
        'public/cpp/application/lib/service_connector.cc',
        'public/cpp/application/lib/service_connector.h',
        'public/cpp/application/lib/service_registry.cc',
        'public/cpp/application/lib/service_registry.h',
        'public/cpp/application/lib/weak_service_provider.cc',
        'public/cpp/application/lib/weak_service_provider.h',
        'public/cpp/application/service_provider_impl.h',
      ],
      'dependencies': [
        'mojo_application_bindings',
      ],
      'export_dependent_settings': [
        'mojo_application_bindings',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application:standalone"
      'target_name': 'mojo_application_standalone',
      'type': 'static_library',
      'sources': [
        'public/cpp/application/lib/application_runner.cc',
        'public/cpp/application/application_runner.h',
      ],
      'dependencies': [
        'mojo_application_base',
        'mojo_environment_standalone',
      ],
      'export_dependent_settings': [
        'mojo_application_base',
      ],
    },
    {
      # GN version: //mojo/public/c/test_support
      'target_name': 'mojo_test_support',
      'defines': [
        'MOJO_TEST_SUPPORT_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'public/c/test_support/test_support.h',
        'public/c/test_support/test_support_export.h',
        # TODO(vtl): Convert this to thunks http://crbug.com/386799
        'public/tests/test_support_private.cc',
        'public/tests/test_support_private.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'type': 'static_library',
        }, {
          'type': 'shared_library',
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
      # GN version: //mojo/public/cpp/test_support:test_utils
      'target_name': 'mojo_public_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_test_support',
      ],
      'sources': [
        'public/cpp/test_support/lib/test_support.cc',
        'public/cpp/test_support/lib/test_utils.cc',
        'public/cpp/test_support/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_test_utils
      'target_name': 'mojo_public_bindings_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'public/cpp/bindings/tests/validation_test_input_parser.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.h',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/bindings/tests/math_calculator.mojom',
          'public/interfaces/bindings/tests/no_module.mojom',
          'public/interfaces/bindings/tests/rect.mojom',
          'public/interfaces/bindings/tests/regression_tests.mojom',
          'public/interfaces/bindings/tests/regression_tests_import.mojom',
          'public/interfaces/bindings/tests/sample_factory.mojom',
          'public/interfaces/bindings/tests/sample_import.mojom',
          'public/interfaces/bindings/tests/sample_import2.mojom',
          'public/interfaces/bindings/tests/sample_interfaces.mojom',
          'public/interfaces/bindings/tests/sample_service.mojom',
          'public/interfaces/bindings/tests/serialization_test_structs.mojom',
          'public/interfaces/bindings/tests/test_structs.mojom',
          'public/interfaces/bindings/tests/validation_test_interfaces.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces
      'target_name': 'mojo_public_test_interfaces',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom',
        'mojo_cpp_bindings',
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
