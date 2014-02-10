{
  'targets': [
    {
      'target_name': 'mojo_system',
      'type': 'shared_library',
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
        'public/system/async_waiter.h',
        'public/system/core.h',
        'public/system/core_cpp.h',
        'public/system/core_private.cc',
        'public/system/core_private.h',
        'public/system/macros.h',
        'public/system/system_export.h',
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
        'public/gles2/gles2.h',
        'public/gles2/gles2_private.cc',
        'public/gles2/gles2_private.h',
        'public/gles2/gles2_export.h',
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
        'public/tests/test_support.cc',
        'public/tests/test_support.h',
      ],
    },
    {
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_environment_standalone',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_sample_service',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/bindings/tests/array_unittest.cc',
        'public/bindings/tests/connector_unittest.cc',
        'public/bindings/tests/handle_passing_unittest.cc',
        'public/bindings/tests/remote_ptr_unittest.cc',
        'public/bindings/tests/type_conversion_unittest.cc',
        'public/bindings/tests/buffer_unittest.cc',
        'public/bindings/tests/math_calculator.mojom',
        'public/bindings/tests/sample_factory.mojom',
        'public/bindings/tests/sample_service_unittest.cc',
        'public/bindings/tests/test_structs.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
    },
    {
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_environment_standalone',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/environment/tests/async_waiter_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_system',
      ],
      'sources': [
        'public/tests/system/core_cpp_unittest.cc',
        'public/tests/system/core_unittest.cc',
        'public/tests/system/core_unittest_pure_c.c',
      ],
    },
    {
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_system',
        'mojo_utility',
      ],
      'sources': [
        'public/utility/tests/run_loop_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
        'mojo_run_all_perftests',
        'mojo_system',
      ],
      'sources': [
        'public/tests/system/core_perftest.cc',
      ],
    },
    {
      'target_name': 'mojo_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/bindings/allocation_scope.h',
        'public/bindings/array.h',
        'public/bindings/buffer.h',
        'public/bindings/error_handler.h',
        'public/bindings/passable.h',
        'public/bindings/remote_ptr.h',
        'public/bindings/sync_dispatcher.h',
        'public/bindings/type_converter.h',
        'public/bindings/lib/array.cc',
        'public/bindings/lib/array_internal.h',
        'public/bindings/lib/array_internal.cc',
        'public/bindings/lib/bindings_internal.h',
        'public/bindings/lib/bindings_serialization.cc',
        'public/bindings/lib/bindings_serialization.h',
        'public/bindings/lib/buffer.cc',
        'public/bindings/lib/connector.cc',
        'public/bindings/lib/connector.h',
        'public/bindings/lib/fixed_buffer.cc',
        'public/bindings/lib/fixed_buffer.h',
        'public/bindings/lib/message.cc',
        'public/bindings/lib/message.h',
        'public/bindings/lib/message_builder.cc',
        'public/bindings/lib/message_builder.h',
        'public/bindings/lib/message_queue.cc',
        'public/bindings/lib/message_queue.h',
        'public/bindings/lib/scratch_buffer.cc',
        'public/bindings/lib/scratch_buffer.h',
        'public/bindings/lib/sync_dispatcher.cc',
      ],
    },
    {
      'target_name': 'mojo_sample_service',
      'type': 'static_library',
      'sources': [
        'public/bindings/tests/sample_service.mojom',
        'public/bindings/tests/sample_import.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/environment/buffer_tls.h',
        'public/environment/default_async_waiter.h',
        'public/environment/environment.h',
        'public/environment/lib/default_async_waiter.cc',
        'public/environment/lib/buffer_tls.cc',
        'public/environment/lib/buffer_tls_setup.h',
        'public/environment/lib/environment.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/utility/lib/run_loop.cc',
        'public/utility/lib/thread_local.h',
        'public/utility/lib/thread_local_posix.cc',
        'public/utility/lib/thread_local_win.cc',
        'public/utility/run_loop.h',
        'public/utility/run_loop_handler.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_shell_bindings',
      'type': 'static_library',
      'sources': [
        'public/shell/lib/shell.mojom',
        'public/shell/lib/service.cc',
        'public/shell/service.h',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
  ],
}
