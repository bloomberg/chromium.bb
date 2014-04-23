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
      # This is linked directly into the embedder, so we make it a component.
      'type': '<(component)',
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
    {
      'target_name': 'mojo_view_manager_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'services/public/cpp/view_manager/lib/view.cc',
        'services/public/cpp/view_manager/lib/view_manager.cc',
        'services/public/cpp/view_manager/lib/view_tree_host.cc',
        'services/public/cpp/view_manager/lib/view_tree_node.cc',
        'services/public/cpp/view_manager/lib/view_tree_node_observer.cc',
        'services/public/cpp/view_manager/lib/view_tree_node_private.cc',
        'services/public/cpp/view_manager/lib/view_tree_node_private.h',
        'services/public/cpp/view_manager/view.h',
        'services/public/cpp/view_manager/view_manager.h',
        'services/public/cpp/view_manager/view_tree_host.h',
        'services/public/cpp/view_manager/view_tree_node.h',
        'services/public/cpp/view_manager/view_tree_node_observer.h',
      ],
    },
    {
      'target_name': 'mojo_view_manager_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        'mojo_view_manager_lib',
      ],
      'sources': [
        'services/public/cpp/view_manager/tests/view_unittest.cc',
        'services/public/cpp/view_manager/tests/view_manager_unittest.cc',
        'services/public/cpp/view_manager/tests/view_tree_host_unittest.cc',
        'services/public/cpp/view_manager/tests/view_tree_node_unittest.cc',
      ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'mojo_view_manager_bindings',
          'type': 'static_library',
          'sources': [
            'services/public/interfaces/view_manager/view_manager.mojom',
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
          'target_name': 'mojo_view_manager',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../ui/aura/aura.gyp:aura',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_launcher_bindings',
            'mojo_native_viewport_bindings',
            'mojo_shell_client',
            'mojo_system_impl',
            'mojo_view_manager_bindings',
          ],
          'sources': [
            'services/view_manager/ids.h',
            'services/view_manager/node.cc',
            'services/view_manager/node.h',
            'services/view_manager/node_delegate.h',
            'services/view_manager/root_node_manager.cc',
            'services/view_manager/root_node_manager.h',
            'services/view_manager/view_manager.cc',
            'services/view_manager/view_manager_connection.cc',
            'services/view_manager/view_manager_connection.h',
            'services/view_manager/view_manager_export.h',
          ],
          'defines': [
            'MOJO_VIEW_MANAGER_IMPLEMENTATION',
          ],
        },
        {
          'target_name': 'mojo_view_manager_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../ui/aura/aura.gyp:aura',
            'mojo_environment_chromium',
            'mojo_run_all_unittests',
            'mojo_shell_client',
            'mojo_system_impl',
            'mojo_view_manager',
            'mojo_view_manager_bindings',
          ],
          'sources': [
            'services/view_manager/view_manager_connection_unittest.cc',
          ],
        },
        {
          'target_name': 'package_mojo_view_manager',
          'variables': {
            'app_name': 'mojo_view_manager',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
      ],
    }],
  ],
}
