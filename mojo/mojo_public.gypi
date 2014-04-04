{
  'targets': [
    {
      'target_name': 'mojo_system',
      'type': '<(component)',
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
      'sources': [
        'public/c/system/async_waiter.h',
        'public/c/system/core.h',
        'public/c/system/macros.h',
        'public/c/system/system_export.h',
        'public/system/core_private.cc',
        'public/system/core_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_gles2',
      'type': 'shared_library',
      'defines': [
        'MOJO_GLES2_IMPLEMENTATION',
        'GLES2_USE_MOJO',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../third_party/khronos/khronos.gyp:khronos_headers'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'sources': [
        'public/c/gles2/gles2.h',
        'public/c/gles2/gles2_export.h',
        'public/gles2/gles2_private.cc',
        'public/gles2/gles2_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
          },
        }],
      ],
    },
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
            'DYLIB_INSTALL_NAME_BASE': '@rpath',
          },
          'direct_dependent_settings': {
            'xcode_settings': {
              # Look for run-path dependent libraries in the loader's directory.
              'LD_RUNPATH_SEARCH_PATHS': [ '@loader_path/.', ],
            },
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
        'mojo_system',
        'mojo_test_support',
      ],
      'sources': [
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
        'mojo_bindings',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_sample_service',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/remote_ptr_unittest.cc',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/interfaces/bindings/tests/math_calculator.mojom',
        'public/interfaces/bindings/tests/sample_factory.mojom',
        'public/interfaces/bindings/tests/sample_interfaces.mojom',
        'public/interfaces/bindings/tests/test_structs.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
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
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/environment/tests/async_waiter_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_system',
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
        'mojo_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_system',
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
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
      ],
    },
    {
      'target_name': 'mojo_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/cpp/bindings/allocation_scope.h',
        'public/cpp/bindings/array.h',
        'public/cpp/bindings/buffer.h',
        'public/cpp/bindings/callback.h',
        'public/cpp/bindings/error_handler.h',
        'public/cpp/bindings/interface.h',
        'public/cpp/bindings/message.h',
        'public/cpp/bindings/passable.h',
        'public/cpp/bindings/remote_ptr.h',
        'public/cpp/bindings/sync_dispatcher.h',
        'public/cpp/bindings/type_converter.h',
        'public/cpp/bindings/lib/array.cc',
        'public/cpp/bindings/lib/array_internal.h',
        'public/cpp/bindings/lib/array_internal.cc',
        'public/cpp/bindings/lib/bindings_internal.h',
        'public/cpp/bindings/lib/bindings_serialization.cc',
        'public/cpp/bindings/lib/bindings_serialization.h',
        'public/cpp/bindings/lib/buffer.cc',
        'public/cpp/bindings/lib/callback_internal.h',
        'public/cpp/bindings/lib/connector.cc',
        'public/cpp/bindings/lib/connector.h',
        'public/cpp/bindings/lib/fixed_buffer.cc',
        'public/cpp/bindings/lib/fixed_buffer.h',
        'public/cpp/bindings/lib/interface.cc',
        'public/cpp/bindings/lib/message.cc',
        'public/cpp/bindings/lib/message_builder.cc',
        'public/cpp/bindings/lib/message_builder.h',
        'public/cpp/bindings/lib/message_internal.h',
        'public/cpp/bindings/lib/message_queue.cc',
        'public/cpp/bindings/lib/message_queue.h',
        'public/cpp/bindings/lib/router.cc',
        'public/cpp/bindings/lib/router.h',
        'public/cpp/bindings/lib/scratch_buffer.cc',
        'public/cpp/bindings/lib/scratch_buffer.h',
        'public/cpp/bindings/lib/shared_data.h',
        'public/cpp/bindings/lib/shared_ptr.h',
        'public/cpp/bindings/lib/sync_dispatcher.cc',
        'public/js/bindings/constants.cc',
        'public/js/bindings/constants.h',
      ],
    },
    {
      'target_name': 'mojo_sample_service',
      'type': 'static_library',
      'sources': [
        'public/interfaces/bindings/tests/sample_service.mojom',
        'public/interfaces/bindings/tests/sample_import.mojom',
        'public/interfaces/bindings/tests/sample_import2.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/cpp/environment/buffer_tls.h',
        'public/cpp/environment/default_async_waiter.h',
        'public/cpp/environment/environment.h',
        'public/cpp/environment/lib/default_async_waiter.cc',
        'public/cpp/environment/lib/buffer_tls.cc',
        'public/cpp/environment/lib/buffer_tls_setup.h',
        'public/cpp/environment/lib/environment.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
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
      'target_name': 'mojo_shell_bindings',
      'type': 'static_library',
      'sources': [
        'public/interfaces/shell/shell.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_bindings',
        'mojo_system',
      ],
      'export_dependent_settings': [
        'mojo_bindings',
      ],
    },
    {
      'target_name': 'mojo_shell_client',
      'type': 'static_library',
      'sources': [
        'public/cpp/shell/application.h',
        'public/cpp/shell/service.h',
        'public/cpp/shell/lib/application.cc',
        'public/cpp/shell/lib/service.cc',
      ],
      'dependencies': [
        'mojo_shell_bindings',
      ],
      'export_dependent_settings': [
        'mojo_shell_bindings',
      ],
    },
  ],
}
