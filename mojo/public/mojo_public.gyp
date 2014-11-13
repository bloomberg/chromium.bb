# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../mojo_variables.gypi',
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
      'target_name': 'mojo_none',
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
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
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
        'c/system/buffer.h',
        'c/system/core.h',
        'c/system/data_pipe.h',
        'c/system/functions.h',
        'c/system/macros.h',
        'c/system/message_pipe.h',
        'c/system/system_export.h',
        'c/system/types.h',
        'platform/native/system_thunks.cc',
        'platform/native/system_thunks.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings
      'target_name': 'mojo_cpp_bindings',
      'type': 'static_library',
      'include_dirs': [
        '../..'
      ],
      'sources': [
        'cpp/bindings/array.h',
        'cpp/bindings/binding.h',
        'cpp/bindings/callback.h',
        'cpp/bindings/error_handler.h',
        'cpp/bindings/interface_impl.h',
        'cpp/bindings/interface_ptr.h',
        'cpp/bindings/interface_request.h',
        'cpp/bindings/message.h',
        'cpp/bindings/message_filter.h',
        'cpp/bindings/no_interface.h',
        'cpp/bindings/string.h',
        'cpp/bindings/strong_binding.h',
        'cpp/bindings/type_converter.h',
        'cpp/bindings/lib/array_internal.h',
        'cpp/bindings/lib/array_internal.cc',
        'cpp/bindings/lib/array_serialization.h',
        'cpp/bindings/lib/bindings_internal.h',
        'cpp/bindings/lib/bindings_serialization.cc',
        'cpp/bindings/lib/bindings_serialization.h',
        'cpp/bindings/lib/bounds_checker.cc',
        'cpp/bindings/lib/bounds_checker.h',
        'cpp/bindings/lib/buffer.h',
        'cpp/bindings/lib/callback_internal.h',
        'cpp/bindings/lib/connector.cc',
        'cpp/bindings/lib/connector.h',
        'cpp/bindings/lib/filter_chain.cc',
        'cpp/bindings/lib/filter_chain.h',
        'cpp/bindings/lib/fixed_buffer.cc',
        'cpp/bindings/lib/fixed_buffer.h',
        'cpp/bindings/lib/interface_ptr_internal.h',
        'cpp/bindings/lib/map_data_internal.h',
        'cpp/bindings/lib/map_internal.h',
        'cpp/bindings/lib/map_serialization.h',
        'cpp/bindings/lib/message.cc',
        'cpp/bindings/lib/message_builder.cc',
        'cpp/bindings/lib/message_builder.h',
        'cpp/bindings/lib/message_filter.cc',
        'cpp/bindings/lib/message_header_validator.cc',
        'cpp/bindings/lib/message_header_validator.h',
        'cpp/bindings/lib/message_internal.h',
        'cpp/bindings/lib/message_queue.cc',
        'cpp/bindings/lib/message_queue.h',
        'cpp/bindings/lib/no_interface.cc',
        'cpp/bindings/lib/router.cc',
        'cpp/bindings/lib/router.h',
        'cpp/bindings/lib/shared_data.h',
        'cpp/bindings/lib/shared_ptr.h',
        'cpp/bindings/lib/string_serialization.h',
        'cpp/bindings/lib/string_serialization.cc',
        'cpp/bindings/lib/validate_params.h',
        'cpp/bindings/lib/validation_errors.cc',
        'cpp/bindings/lib/validation_errors.h',
      ],
    },
    {
      # GN version: //mojo/public/js
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '../..'
      ],
      'sources': [
        'js/constants.cc',
        'js/constants.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment:standalone
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'c/environment/async_waiter.h',
        'c/environment/logger.h',
        'cpp/environment/async_waiter.h',
        'cpp/environment/environment.h',
        'cpp/environment/lib/async_waiter.cc',
        'cpp/environment/lib/default_async_waiter.cc',
        'cpp/environment/lib/default_async_waiter.h',
        'cpp/environment/lib/default_logger.cc',
        'cpp/environment/lib/default_logger.h',
        'cpp/environment/lib/environment.cc',
        'cpp/environment/lib/logging.cc',
        'cpp/environment/logging.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # GN version: //mojo/public/cpp/utility
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'cpp/utility/mutex.h',
        'cpp/utility/run_loop.h',
        'cpp/utility/run_loop_handler.h',
        'cpp/utility/thread.h',
        'cpp/utility/lib/mutex.cc',
        'cpp/utility/lib/run_loop.cc',
        'cpp/utility/lib/thread.cc',
        'cpp/utility/lib/thread_local.h',
        'cpp/utility/lib/thread_local_posix.cc',
        'cpp/utility/lib/thread_local_win.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'cpp/utility/mutex.h',
            'cpp/utility/thread.h',
            'cpp/utility/lib/mutex.cc',
            'cpp/utility/lib/thread.cc',
          ],
        }],
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      'target_name': 'mojo_application_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'interfaces/application/application.mojom',
          'interfaces/application/service_provider.mojom',
          'interfaces/application/shell.mojom',
        ],
      },
      'includes': [ 'tools/bindings/mojom_bindings_generator_explicit.gypi' ],
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
        'cpp/application/application_connection.h',
        'cpp/application/application_delegate.h',
        'cpp/application/application_impl.h',
        'cpp/application/connect.h',
        'cpp/application/interface_factory.h',
        'cpp/application/interface_factory_impl.h',
        'cpp/application/lib/application_connection.cc',
        'cpp/application/lib/application_delegate.cc',
        'cpp/application/lib/application_impl.cc',
        'cpp/application/lib/service_provider_impl.cc',
        'cpp/application/lib/service_connector.cc',
        'cpp/application/lib/service_connector.h',
        'cpp/application/lib/service_registry.cc',
        'cpp/application/lib/service_registry.h',
        'cpp/application/lib/weak_service_provider.cc',
        'cpp/application/lib/weak_service_provider.h',
        'cpp/application/service_provider_impl.h',
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
        'cpp/application/lib/application_runner.cc',
        'cpp/application/application_runner.h',
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
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },
      'sources': [
        'c/test_support/test_support.h',
        'c/test_support/test_support_export.h',
        # TODO(vtl): Convert this to thunks http://crbug.com/386799
        'tests/test_support_private.cc',
        'tests/test_support_private.h',
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
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_test_support',
      ],
      'sources': [
        'cpp/test_support/lib/test_support.cc',
        'cpp/test_support/lib/test_utils.cc',
        'cpp/test_support/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_test_utils
      'target_name': 'mojo_public_bindings_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'cpp/bindings/tests/validation_test_input_parser.cc',
        'cpp/bindings/tests/validation_test_input_parser.h',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'interfaces/bindings/tests/math_calculator.mojom',
          'interfaces/bindings/tests/no_module.mojom',
          'interfaces/bindings/tests/rect.mojom',
          'interfaces/bindings/tests/regression_tests.mojom',
          'interfaces/bindings/tests/sample_factory.mojom',
          'interfaces/bindings/tests/sample_import.mojom',
          'interfaces/bindings/tests/sample_import2.mojom',
          'interfaces/bindings/tests/sample_interfaces.mojom',
          'interfaces/bindings/tests/sample_service.mojom',
          'interfaces/bindings/tests/serialization_test_structs.mojom',
          'interfaces/bindings/tests/test_structs.mojom',
          'interfaces/bindings/tests/validation_test_interfaces.mojom',
        ],
      },
      'includes': [ 'tools/bindings/mojom_bindings_generator_explicit.gypi' ],
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
            'java_in_dir': 'java/system',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
        # GN version: //mojo/public/java_bindings
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'java/bindings',
           },
           'dependencies': [
             'mojo_public_java',
           ],
           'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
