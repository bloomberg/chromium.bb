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
        'public/tests/bindings/array_unittest.cc',
        'public/tests/bindings/connector_unittest.cc',
        'public/tests/bindings/handle_passing_unittest.cc',
        'public/tests/bindings/remote_ptr_unittest.cc',
        'public/tests/bindings/type_conversion_unittest.cc',
        'public/tests/bindings/buffer_unittest.cc',
        'public/tests/bindings/math_calculator.mojom',
        'public/tests/bindings/sample_factory.mojom',
        'public/tests/bindings/sample_service_unittest.cc',
        'public/tests/bindings/test_structs.mojom',
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
        'public/tests/environment/async_waiter_unittest.cc',
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
        'public/tests/utility/run_loop_unittest.cc',
        'public/tests/utility/thread_local_unittest.cc',
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
        'public/bindings/lib/array.cc',
        'public/bindings/lib/array.h',
        'public/bindings/lib/array_internal.h',
        'public/bindings/lib/array_internal.cc',
        'public/bindings/lib/bindings.h',
        'public/bindings/lib/bindings_internal.h',
        'public/bindings/lib/bindings_serialization.cc',
        'public/bindings/lib/bindings_serialization.h',
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
      'target_name': 'mojo_sample_service',
      'type': 'static_library',
      'sources': [
        'public/tests/bindings/sample_service.mojom',
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
        'public/environment/default_async_waiter.h',
        'public/environment/buffer_tls.h',
        'public/environment/environment.h',
        'public/environment/standalone/default_async_waiter.cc',
        'public/environment/standalone/buffer_tls.cc',
        'public/environment/standalone/buffer_tls_setup.h',
        'public/environment/standalone/environment.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/utility/run_loop.cc',
        'public/utility/run_loop.h',
        'public/utility/run_loop_handler.h',
        'public/utility/thread_local.h',
        'public/utility/thread_local_posix.cc',
        'public/utility/thread_local_win.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
