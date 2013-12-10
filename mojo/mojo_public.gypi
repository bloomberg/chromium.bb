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
        'public/tests/simple_bindings_support.cc',
        'public/tests/simple_bindings_support.h',
        'public/tests/test_support.cc',
        'public/tests/test_support.h',
      ],
    },
    {
      'target_name': 'mojo_public_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_bindings',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'mojo_system',
      ],
      'sources': [
        'public/tests/bindings_array_unittest.cc',
        'public/tests/bindings_connector_unittest.cc',
        'public/tests/bindings_handle_passing_unittest.cc',
        'public/tests/bindings_remote_ptr_unittest.cc',
        'public/tests/bindings_type_conversion_unittest.cc',
        'public/tests/buffer_unittest.cc',
        'public/tests/math_calculator.mojom',
        'public/tests/sample_factory.mojom',
        'public/tests/system_core_cpp_unittest.cc',
        'public/tests/system_core_unittest.cc',
        'public/tests/test_structs.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
    },
    {
      'target_name': 'mojo_public_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
        'mojo_run_all_perftests',
        'mojo_system',
      ],
      'sources': [
        'public/tests/system_core_perftest.cc',
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
      'target_name': 'sample_service',
      'type': 'static_library',
      'sources': [
        'public/bindings/sample/sample_service.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'mojo_bindings_unittests',
      'type': 'executable',
      'sources': [
        'public/bindings/sample/sample_service_unittests.cc',
      ],
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
        'mojo_run_all_unittests',
        'sample_service',
      ],
    },
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/utility/bindings_support_impl.cc',
        'public/utility/bindings_support_impl.h',
        'public/utility/environment.cc',
        'public/utility/environment.h',
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
    {
      'target_name': 'mojo_utility_unittests',
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
        'public/utility/bindings_support_impl_unittest.cc',
        'public/utility/run_loop_unittest.cc',
        'public/utility/thread_local_unittest.cc',
      ],
    },
  ],
}
