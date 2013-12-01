{
  'targets': [
    {
      'target_name': 'sample_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:gles2_c_lib',
        '../ui/gl/gl.gyp:gl',
        'gles2',
        'gles2_client_impl',
        'mojo_common_lib',
        'mojo_system',
        'native_viewport',
      ],
      'sources': [
        'examples/sample_app/native_viewport_client_impl.cc',
        'examples/sample_app/native_viewport_client_impl.h',
        'examples/sample_app/sample_app.cc',
        'examples/sample_app/sample_gles2_delegate.cc',
        'examples/sample_app/sample_gles2_delegate.h',
        'examples/sample_app/spinning_cube.cc',
        'examples/sample_app/spinning_cube.h',
      ],
    },
    {
      'target_name': 'hello_world_service',
      'type': 'static_library',
      'sources': [
        'examples/hello_world_service/hello_world_service.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'hello_world_service_impl',
      'type': 'static_library',
      'sources': [
        'examples/hello_world_service/hello_world_service_impl.cc',
        'examples/hello_world_service/hello_world_service_impl.h',
      ],
      'export_dependent_settings': [
        'hello_world_service',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'hello_world_service',
      ],
    },
  ],
}
