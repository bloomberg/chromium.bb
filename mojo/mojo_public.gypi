{
  'targets': [
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
        'public/tests/bindings_connector_unittest.cc',
        'public/tests/bindings_remote_ptr_unittest.cc',
        'public/tests/bindings_type_conversion_unittest.cc',
        'public/tests/buffer_unittest.cc',
        'public/tests/math_calculator.mojom',
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
        'public/bindings/lib/bindings.cc',
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
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'dependencies': [
        '../gin/gin.gyp:gin',
        'mojo_system',
      ],
      'export_dependent_settings': [
        '../gin/gin.gyp:gin',
      ],
      'sources': [
        'public/bindings/js/core.cc',
        'public/bindings/js/core.h',
        'public/bindings/js/handle.cc',
        'public/bindings/js/handle.h',
        'public/bindings/js/support.cc',
        'public/bindings/js/support.h',
        'public/bindings/js/waiting_callback.cc',
        'public/bindings/js/waiting_callback.h',
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
      'target_name': 'mojo_js_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_js_bindings',
        'mojo_run_all_unittests',
        'sample_service',
      ],
      'sources': [
        'public/bindings/js/test/run_js_tests.cc',
      ],
    },
    {
      'target_name': 'gles2_client_impl',
      'type': 'static_library',
      'dependencies': [
        '../gpu/gpu.gyp:gles2_c_lib',
        'gles2',
      ],
      'export_dependent_settings': [
        'gles2',
      ],
      'sources': [
        'public/bindings/gles2_client/gles2_client_impl.cc',
        'public/bindings/gles2_client/gles2_client_impl.h',
      ],
    },
  ],
}
