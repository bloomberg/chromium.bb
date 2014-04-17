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
        'mojo_system_impl',
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
    {
      'target_name': 'mojo_pepper_container_app',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gpu/gpu.gyp:command_buffer_common',
        '../ppapi/ppapi.gyp:ppapi_c',
        '../ppapi/ppapi_internal.gyp:ppapi_example_gles2_spinning_cube',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_gles2',
        'mojo_native_viewport_bindings',
        'mojo_shell_client',
        'mojo_system_impl',
      ],
      'defines': [
        # We don't really want to export. We could change how
        # ppapi_{shared,thunk}_export.h are defined to avoid this.
        'PPAPI_SHARED_IMPLEMENTATION',
        'PPAPI_THUNK_IMPLEMENTATION',
      ],
      'sources': [
        # Source files from ppapi/.
        # An alternative is to depend on
        # '../ppapi/ppapi_internal.gyp:ppapi_shared', but that target includes
        # a lot of things that we don't need.
        # TODO(yzshen): Consider extracting these files into a separate target
        # which mojo_pepper_container_app and ppapi_shared both depend on.
        '../ppapi/shared_impl/api_id.h',
        '../ppapi/shared_impl/callback_tracker.cc',
        '../ppapi/shared_impl/callback_tracker.h',
        '../ppapi/shared_impl/host_resource.cc',
        '../ppapi/shared_impl/host_resource.h',
        '../ppapi/shared_impl/id_assignment.cc',
        '../ppapi/shared_impl/id_assignment.h',
        '../ppapi/shared_impl/ppapi_globals.cc',
        '../ppapi/shared_impl/ppapi_globals.h',
        '../ppapi/shared_impl/ppapi_shared_export.h',
        '../ppapi/shared_impl/ppb_message_loop_shared.cc',
        '../ppapi/shared_impl/ppb_message_loop_shared.h',
        '../ppapi/shared_impl/ppb_view_shared.cc',
        '../ppapi/shared_impl/ppb_view_shared.h',
        '../ppapi/shared_impl/proxy_lock.cc',
        '../ppapi/shared_impl/proxy_lock.h',
        '../ppapi/shared_impl/resource.cc',
        '../ppapi/shared_impl/resource.h',
        '../ppapi/shared_impl/resource_tracker.cc',
        '../ppapi/shared_impl/resource_tracker.h',
        '../ppapi/shared_impl/scoped_pp_resource.cc',
        '../ppapi/shared_impl/scoped_pp_resource.h',
        '../ppapi/shared_impl/singleton_resource_id.h',
        '../ppapi/shared_impl/tracked_callback.cc',
        '../ppapi/shared_impl/tracked_callback.h',
        '../ppapi/thunk/enter.cc',
        '../ppapi/thunk/enter.h',
        '../ppapi/thunk/interfaces_ppb_private.h',
        '../ppapi/thunk/interfaces_ppb_private_flash.h',
        '../ppapi/thunk/interfaces_ppb_private_no_permissions.h',
        '../ppapi/thunk/interfaces_ppb_public_dev.h',
        '../ppapi/thunk/interfaces_ppb_public_dev_channel.h',
        '../ppapi/thunk/interfaces_ppb_public_stable.h',
        '../ppapi/thunk/interfaces_preamble.h',
        '../ppapi/thunk/ppapi_thunk_export.h',
        '../ppapi/thunk/ppb_graphics_3d_api.h',
        '../ppapi/thunk/ppb_graphics_3d_thunk.cc',
        '../ppapi/thunk/ppb_instance_api.h',
        '../ppapi/thunk/ppb_instance_thunk.cc',
        '../ppapi/thunk/ppb_message_loop_api.h',
        '../ppapi/thunk/ppb_view_api.h',
        '../ppapi/thunk/ppb_view_thunk.cc',
        '../ppapi/thunk/resource_creation_api.h',
        '../ppapi/thunk/thunk.h',

        'examples/pepper_container_app/graphics_3d_resource.cc',
        'examples/pepper_container_app/graphics_3d_resource.h',
        'examples/pepper_container_app/interface_list.cc',
        'examples/pepper_container_app/interface_list.h',
        'examples/pepper_container_app/mojo_ppapi_globals.cc',
        'examples/pepper_container_app/mojo_ppapi_globals.h',
        'examples/pepper_container_app/pepper_container_app.cc',
        'examples/pepper_container_app/plugin_instance.cc',
        'examples/pepper_container_app/plugin_instance.h',
        'examples/pepper_container_app/plugin_module.cc',
        'examples/pepper_container_app/plugin_module.h',
        'examples/pepper_container_app/ppb_core_thunk.cc',
        'examples/pepper_container_app/ppb_opengles2_thunk.cc',
        'examples/pepper_container_app/resource_creation_impl.cc',
        'examples/pepper_container_app/resource_creation_impl.h',
        'examples/pepper_container_app/thunk.h',
        'examples/pepper_container_app/type_converters.h',
      ],
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
            'mojo_system_impl'
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
            'mojo_system_impl',
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
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
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
