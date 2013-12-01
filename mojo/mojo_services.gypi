{
  'targets': [
    {
      'target_name': 'gles2',
      'type': 'static_library',
      'sources': [
        'services/gles2/gles2.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'gles2_impl',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:command_buffer_service',
        '../gpu/gpu.gyp:gles2_implementation',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gl/gl.gyp:gl',
        'gles2',
      ],
      'export_dependent_settings': [
        'gles2',
      ],
      'sources': [
        'services/gles2/gles2_impl.cc',
        'services/gles2/gles2_impl.h',
      ],
    },
    {
      'target_name': 'native_viewport',
      'type': 'static_library',
      'sources': [
        'services/native_viewport/native_viewport.mojom',
      ],
      'includes': [ 'public/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_bindings',
        'mojo_system',
      ],
    },
    {
      'target_name': 'native_viewport_impl',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        'gles2_impl',
        'native_viewport',
      ],
      'export_dependent_settings': [
        'native_viewport',
      ],
      'sources': [
        'services/native_viewport/android/mojo_viewport.cc',
        'services/native_viewport/android/mojo_viewport.h',
        'services/native_viewport/native_viewport.h',
        'services/native_viewport/native_viewport_android.cc',
        'services/native_viewport/native_viewport_impl.cc',
        'services/native_viewport/native_viewport_impl.h',
        'services/native_viewport/native_viewport_mac.mm',
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
