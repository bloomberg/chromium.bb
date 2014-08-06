# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mojo_test_support',
      'type': 'shared_library',
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
        'public/tests/test_support_private.cc',
        'public/tests/test_support_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
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
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_public_test_interfaces',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/bounds_checker_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/interface_ptr_unittest.cc',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/string_unittest.cc',
        'public/cpp/bindings/tests/struct_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.h',
        'public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/environment/tests/async_waiter_unittest.cc',
        'public/cpp/environment/tests/logger_unittest.cc',
        'public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_application_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_application_standalone',
        'mojo_utility',
        'mojo_environment_standalone',
        'mojo_run_all_unittests',
      ],
      'sources': [
        'public/cpp/application/tests/service_registry_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
      ],
      'sources': [
        'public/c/system/tests/core_unittest.cc',
        'public/c/system/tests/core_unittest_pure_c.c',
        'public/c/system/tests/macros_unittest.cc',
        'public/cpp/system/tests/core_unittest.cc',
        'public/cpp/system/tests/macros_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/utility/tests/mutex_unittest.cc',
        'public/cpp/utility/tests/run_loop_unittest.cc',
        'public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/tests/mutex_unittest.cc',
            'public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_utils',
        'mojo_run_all_perftests',
        'mojo_utility',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces',
      'type': 'static_library',
      'sources': [
        'public/interfaces/bindings/tests/math_calculator.mojom',
        'public/interfaces/bindings/tests/sample_factory.mojom',
        'public/interfaces/bindings/tests/sample_import.mojom',
        'public/interfaces/bindings/tests/sample_import2.mojom',
        'public/interfaces/bindings/tests/sample_interfaces.mojom',
        'public/interfaces/bindings/tests/sample_service.mojom',
        'public/interfaces/bindings/tests/test_structs.mojom',
        'public/interfaces/bindings/tests/validation_test_interfaces.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_common_test_support',
        'mojo_js_bindings_lib',
        'mojo_public_test_interfaces',
        'mojo_run_all_unittests',
      ],
      'sources': [
        'public/js/bindings/tests/run_js_tests.cc',
      ],
    },
  ],
}
