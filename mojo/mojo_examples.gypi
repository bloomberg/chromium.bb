{
  'targets': [
    {
      'target_name': 'mojo_sample_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:gles2_c_lib',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gl/gl.gyp:gl',
        'mojo_common_lib',
        'mojo_gles2',
        'mojo_gles2_bindings',
        'mojo_native_viewport_bindings',
        'mojo_shell_bindings',
        'mojo_system',
      ],
      'sources': [
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/gles2_client_impl.cc',
        'examples/sample_app/sample_app.cc',
        'examples/sample_app/spinning_cube.cc',
        'examples/sample_app/spinning_cube.h',
      ],
    },
    {
      'target_name': 'package_mojo_sample_app',
      'variables': {
        'app_name': 'mojo_sample_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
    {
      'target_name': 'mojo_hello_world_bindings',
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
      'target_name': 'mojo_hello_world_service',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_hello_world_bindings',
      ],
      'export_dependent_settings': [
        'mojo_hello_world_bindings',
      ],
      'sources': [
        'examples/hello_world_service/hello_world_service_impl.cc',
        'examples/hello_world_service/hello_world_service_impl.h',
      ],
    },
  ],
}
