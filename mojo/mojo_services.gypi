{
  'targets': [
    {
      'target_name': 'mojo_gles2_bindings',
      'type': 'static_library',
      'sources': [
        'services/gles2/command_buffer.mojom',
        'services/gles2/command_buffer_type_conversions.cc',
        'services/gles2/command_buffer_type_conversions.h',
        'services/gles2/mojo_buffer_backing.cc',
        'services/gles2/mojo_buffer_backing.h',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
      ],
      'dependencies': [
        '../gpu/gpu.gyp:command_buffer_common',
        'mojo_bindings',
      ],
    },
    {
      'target_name': 'mojo_gles2_service',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:command_buffer_service',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_gles2_bindings',
      ],
      'export_dependent_settings': [
        'mojo_gles2_bindings',
      ],
      'sources': [
        'services/gles2/command_buffer_impl.cc',
        'services/gles2/command_buffer_impl.h',
      ],
    },
    {
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'sources': [
        'services/native_viewport/native_viewport.mojom',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
      ],
      'dependencies': [
        'mojo_bindings',
      ],
    },
    {
      'target_name': 'mojo_native_viewport_service',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_gles2_service',
        'mojo_native_viewport_bindings',
        'mojo_shell_client',
        'mojo_system_impl',
      ],
      'defines': [
        'MOJO_NATIVE_VIEWPORT_IMPLEMENTATION',
      ],
      'sources': [
        'services/native_viewport/geometry_conversions.h',
        'services/native_viewport/native_viewport.h',
        'services/native_viewport/native_viewport_android.cc',
        'services/native_viewport/native_viewport_mac.mm',
        'services/native_viewport/native_viewport_service.cc',
        'services/native_viewport/native_viewport_service.h',
        'services/native_viewport/native_viewport_stub.cc',
        'services/native_viewport/native_viewport_win.cc',
        'services/native_viewport/native_viewport_x11.cc',
      ],
      'conditions': [
        ['OS=="win" or OS=="android" or OS=="linux" or OS=="mac"', {
          'sources!': [
            'services/native_viewport/native_viewport_stub.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'mojo_jni_headers',
          ],
        }],
      ],
    },
  ],
}
