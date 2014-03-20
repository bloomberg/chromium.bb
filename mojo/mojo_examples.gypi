# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mojo_sample_app',
      'type': 'shared_library',
      'dependencies': [
        # TODO(darin): we should not be linking against these libraries!
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_bindings',
        'mojo_environment_standalone',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_shell_client',
        'mojo_system',
        'mojo_utility',
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
      'target_name': 'mojo_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../skia/skia.gyp:skia',
        '../gpu/gpu.gyp:gles2_implementation',
        'mojo_gles2',
        'mojo_system',
      ],
      'sources': [
        'examples/compositor_app/mojo_context_provider.cc',
        'examples/compositor_app/mojo_context_provider.h',
      ],
    },
    {
      'target_name': 'mojo_compositor_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_common_lib',
        'mojo_compositor_support',
        'mojo_environment_chromium',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_shell_client',
        'mojo_system',
      ],
      'sources': [
        'examples/compositor_app/compositor_app.cc',
        'examples/compositor_app/compositor_host.cc',
        'examples/compositor_app/compositor_host.h',
      ],
    },
    {
      'target_name': 'package_mojo_compositor_app',
      'variables': {
        'app_name': 'mojo_compositor_app',
      },
      'includes': [ 'build/package_app.gypi' ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'mojo_aura_demo_support',
          'type': 'static_library',
          'dependencies': [
            '../cc/cc.gyp:cc',
            '../ui/aura/aura.gyp:aura',
            '../ui/events/events.gyp:events',
            '../ui/events/events.gyp:events_base',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/gl/gl.gyp:gl',
            '../webkit/common/gpu/webkit_gpu.gyp:webkit_gpu',
            'mojo_compositor_support',
            'mojo_gles2',
            'mojo_native_viewport_bindings',
          ],
          'sources': [
            'examples/aura_demo/demo_context_factory.cc',
            'examples/aura_demo/demo_context_factory.h',
            'examples/aura_demo/demo_screen.cc',
            'examples/aura_demo/demo_screen.h',
            'examples/aura_demo/window_tree_host_mojo.cc',
            'examples/aura_demo/window_tree_host_mojo.h',
          ],
        },
        {
          'target_name': 'mojo_aura_demo',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_aura_demo_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_gles2',
            'mojo_shell_client',
            'mojo_system',
          ],
          'sources': [
            'examples/aura_demo/aura_demo.cc',
          ],
        },
        {
          'target_name': 'package_mojo_aura_demo',
          'variables': {
            'app_name': 'mojo_aura_demo',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
        {
          'target_name': 'mojo_launcher_bindings',
          'type': 'static_library',
          'sources': [
            'examples/launcher/launcher.mojom',
          ],
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
          'target_name': 'mojo_launcher',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../ui/aura/aura.gyp:aura',
            '../ui/aura/aura.gyp:aura_test_support',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/views/views.gyp:views',
            '../url/url.gyp:url_lib',
            'mojo_aura_demo_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_gles2',
            'mojo_launcher_bindings',
            'mojo_shell_client',
            'mojo_system',
          ],
          'sources': [
            'examples/launcher/launcher.cc',
          ],
        },
        {
          'target_name': 'package_mojo_launcher',
          'variables': {
            'app_name': 'mojo_launcher',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
        {
          'target_name': 'mojo_view_manager_bindings',
          'type': 'static_library',
          'sources': [
            'examples/view_manager/view_manager.mojom',
          ],
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
          'target_name': 'mojo_view_manager',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_launcher_bindings',
            'mojo_native_viewport_bindings',
            'mojo_shell_client',
            'mojo_system',
            'mojo_view_manager_bindings',
          ],
          'sources': [
            'examples/view_manager/view_manager.cc',
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
