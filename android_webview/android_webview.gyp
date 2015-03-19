# Copyright 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'android_webview_pak',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_android_webview_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/android_webview_assets/webviewchromium.pak',
          },
         'includes': [ '../build/repack_action.gypi' ],
        },
      ],
      'conditions': [
        ['android_webview_build==0', {
          'actions': [
            {
              'action_name': 'android_webview_locales_rename_paks',
              'variables': {
                'rename_locales': 'tools/webview_locales_rename_paks.py',
              },
              'inputs': [
                '<(rename_locales)',
                '<!@pymod_do_main(webview_locales_rename_paks -i -p <(PRODUCT_DIR) -s <(SHARED_INTERMEDIATE_DIR) <(locales))'
              ],
              'outputs': [
                '<!@pymod_do_main(webview_locales_rename_paks -o -p <(PRODUCT_DIR) -s <(SHARED_INTERMEDIATE_DIR) <(locales))'
              ],
              'action': [
                'python',
                '<(rename_locales)',
                '-p', '<(PRODUCT_DIR)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            }
          ],
        }],
        ['v8_use_external_startup_data==1', {
          'variables': {
            'conditions': [
              ['(target_arch=="arm" or target_arch=="ia32" or target_arch=="mipsel")', {
                'arch_suffix':'32'
              }],
              ['(target_arch=="arm64" or target_arch=="x64" or target_arch=="mips64el")', {
                'arch_suffix':'64'
              }],
            ],
          },
          'actions': [
            {
              'action_name': 'rename_snapshot_blob',
              'inputs': [
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/snapshot_blob_<(arch_suffix).bin',
              ],
              'action': [
                'python',
                '<(DEPTH)/build/cp.py',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            },
            {
              'action_name': 'rename_natives_blob',
              'inputs': [
                '<(PRODUCT_DIR)/natives_blob.bin',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/natives_blob_<(arch_suffix).bin',
              ],
              'action': [
                'python',
                '<(DEPTH)/build/cp.py',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'android_webview_strings_grd',
      'android_unmangled_name': 1,
      'type': 'none',
      'variables': {
        'grd_file': '../android_webview/java/strings/android_webview_strings.grd',
      },
      'includes': [
          '../build/java_strings_grd.gypi',
      ],
    },
    {
      'target_name': 'android_webview_version',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      # Because generate_version generates a header, we must set the
      # hard_dependency flag.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_version',
          'includes': [
            '../build/util/version.gypi',
          ],
          'variables': {
            'template_input_path': 'common/aw_version_info_values.h.version',
          },
          'inputs': [
            '<(version_py_path)',
            '<(template_input_path)',
            '<(version_path)',
            '<(lastchange_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/android_webview/common/aw_version_info_values.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(lastchange_path)',
            '<(template_input_path)',
            '<@(_outputs)',
          ],
          'message': 'Generating version information',
        },
      ],
    },
    {
      'target_name': 'android_webview_common',
      'type': 'static_library',
      'dependencies': [
        '../android_webview/native/webview_native.gyp:webview_native',
        '../components/components.gyp:auto_login_parser',
        '../components/components.gyp:autofill_content_renderer',
        '../components/components.gyp:cdm_browser',
        '../components/components.gyp:cdm_renderer',
        '../components/components.gyp:data_reduction_proxy_core_browser',
        '../components/components.gyp:navigation_interception',
        '../components/components.gyp:printing_common',
        '../components/components.gyp:printing_renderer',
        '../components/components.gyp:visitedlink_browser',
        '../components/components.gyp:visitedlink_renderer',
        '../components/components.gyp:web_contents_delegate_android',
        '../content/content.gyp:content_app_both',
        '../content/content.gyp:content_browser',
        '../gin/gin.gyp:gin',
        '../gpu/blink/gpu_blink.gyp:gpu_blink',
        '../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../gpu/gpu.gyp:command_buffer_service',
        '../gpu/gpu.gyp:gles2_implementation',
        '../gpu/gpu.gyp:gl_in_process_context',
        '../media/media.gyp:media',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../ui/gl/gl.gyp:gl',
        '../ui/shell_dialogs/shell_dialogs.gyp:shell_dialogs',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/common/gpu/webkit_gpu.gyp:webkit_gpu',
        'android_webview_pak',
        'android_webview_version',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/ui/resources/',
      ],
      'sources': [
        'browser/aw_browser_context.cc',
        'browser/aw_browser_context.h',
        'browser/aw_browser_main_parts.cc',
        'browser/aw_browser_main_parts.h',
        'browser/aw_browser_permission_request_delegate.h',
        'browser/aw_contents_client_bridge_base.cc',
        'browser/aw_contents_client_bridge_base.h',
        'browser/aw_content_browser_client.cc',
        'browser/aw_content_browser_client.h',
        'browser/aw_contents_io_thread_client.h',
        'browser/aw_cookie_access_policy.cc',
        'browser/aw_cookie_access_policy.h',
        'browser/aw_dev_tools_manager_delegate.cc',
        'browser/aw_dev_tools_manager_delegate.h',
        'browser/aw_download_manager_delegate.cc',
        'browser/aw_download_manager_delegate.h',
        'browser/aw_form_database_service.cc',
        'browser/aw_form_database_service.h',
        'browser/aw_gl_surface.cc',
        'browser/aw_gl_surface.h',
        'browser/aw_http_auth_handler_base.cc',
        'browser/aw_http_auth_handler_base.h',
        'browser/aw_javascript_dialog_manager.cc',
        'browser/aw_javascript_dialog_manager.h',
        'browser/aw_login_delegate.cc',
        'browser/aw_login_delegate.h',
        'browser/aw_message_port_message_filter.cc',
        'browser/aw_message_port_message_filter.h',
        'browser/aw_message_port_service.h',
        'browser/aw_pref_store.cc',
        'browser/aw_pref_store.h',
        'browser/aw_printing_message_filter.cc',
        'browser/aw_printing_message_filter.h',
        'browser/aw_quota_manager_bridge.cc',
        'browser/aw_quota_manager_bridge.h',
        'browser/aw_quota_permission_context.cc',
        'browser/aw_quota_permission_context.h',
        'browser/aw_request_interceptor.cc',
        'browser/aw_request_interceptor.h',
        'browser/aw_resource_context.cc',
        'browser/aw_resource_context.h',
        'browser/aw_ssl_host_state_delegate.cc',
        'browser/aw_ssl_host_state_delegate.h',
        'browser/aw_result_codes.h',
        'browser/aw_web_preferences_populater.cc',
        'browser/aw_web_preferences_populater.h',
        'browser/aw_web_resource_response.cc',
        'browser/aw_web_resource_response.h',
        'browser/browser_view_renderer.cc',
        'browser/browser_view_renderer.h',
        'browser/browser_view_renderer_client.h',
        'browser/child_frame.cc',
        'browser/child_frame.h',
        'browser/deferred_gpu_command_service.cc',
        'browser/deferred_gpu_command_service.h',
        'browser/find_helper.cc',
        'browser/find_helper.h',
        'browser/hardware_renderer.cc',
        'browser/hardware_renderer.h',
        'browser/icon_helper.cc',
        'browser/icon_helper.h',
        'browser/input_stream.h',
        'browser/jni_dependency_factory.h',
        'browser/gl_view_renderer_manager.cc',
        'browser/gl_view_renderer_manager.h',
        'browser/net/android_stream_reader_url_request_job.cc',
        'browser/net/android_stream_reader_url_request_job.h',
        'browser/net/aw_network_delegate.cc',
        'browser/net/aw_network_delegate.h',
        'browser/net/aw_url_request_context_getter.cc',
        'browser/net/aw_url_request_context_getter.h',
        'browser/net/aw_url_request_job_factory.cc',
        'browser/net/aw_url_request_job_factory.h',
        'browser/net_disk_cache_remover.cc',
        'browser/net_disk_cache_remover.h',
        'browser/net/init_native_callback.h',
        'browser/net/input_stream_reader.cc',
        'browser/net/input_stream_reader.h',
        'browser/parent_compositor_draw_constraints.cc',
        'browser/parent_compositor_draw_constraints.h',
        'browser/parent_output_surface.cc',
        'browser/parent_output_surface.h',
        'browser/renderer_host/aw_render_view_host_ext.cc',
        'browser/renderer_host/aw_render_view_host_ext.h',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.cc',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.h',
        'browser/renderer_host/print_manager.cc',
        'browser/renderer_host/print_manager.h',
        'browser/scoped_allow_wait_for_legacy_web_view_api.h',
        'browser/scoped_app_gl_state_restore.cc',
        'browser/scoped_app_gl_state_restore.h',
        'browser/shared_renderer_state.cc',
        'browser/shared_renderer_state.h',
        'common/android_webview_message_generator.cc',
        'common/android_webview_message_generator.h',
        'common/aw_content_client.cc',
        'common/aw_content_client.h',
        'common/aw_crash_handler.cc',
        'common/aw_crash_handler.h',
        'common/aw_hit_test_data.cc',
        'common/aw_hit_test_data.h',
        'common/aw_message_port_messages.h',
        'common/aw_resource.h',
        'common/aw_switches.cc',
        'common/aw_switches.h',
        'common/devtools_instrumentation.h',
        'common/render_view_messages.cc',
        'common/render_view_messages.h',
        'common/url_constants.cc',
        'common/url_constants.h',
        'crash_reporter/aw_microdump_crash_reporter.h',
        'lib/aw_browser_dependency_factory_impl.cc',
        'lib/aw_browser_dependency_factory_impl.h',
        'lib/main/aw_main_delegate.cc',
        'lib/main/aw_main_delegate.h',
        'lib/main/webview_jni_onload.cc',
        'lib/main/webview_jni_onload.h',
        'public/browser/draw_gl.h',
        'renderer/aw_content_renderer_client.cc',
        'renderer/aw_content_renderer_client.h',
        'renderer/aw_content_settings_client.cc',
        'renderer/aw_content_settings_client.h',
        'renderer/aw_key_systems.cc',
        'renderer/aw_key_systems.h',
        'renderer/aw_message_port_client.cc',
        'renderer/aw_message_port_client.h',
        'renderer/aw_print_web_view_helper_delegate.cc',
        'renderer/aw_print_web_view_helper_delegate.h',
        'renderer/aw_render_process_observer.cc',
        'renderer/aw_render_process_observer.h',
        'renderer/aw_render_frame_ext.cc',
        'renderer/aw_render_frame_ext.h',
        'renderer/aw_render_view_ext.cc',
        'renderer/aw_render_view_ext.h',
        'renderer/print_render_frame_observer.cc',
        'renderer/print_render_frame_observer.h',
      ],
      # TODO(primiano): remove the *_disabled_in_android_builds fallback and
      # merge this with the target once android_webview_build goes away.
      'conditions': [
        ['android_webview_build==0', {
          'dependencies': [
            '../components/components.gyp:breakpad_host',
            '../components/components.gyp:crash_component',
          ],
          'sources': [
            'crash_reporter/aw_microdump_crash_reporter.cc',
          ],
        }, {  # android_webview_build==1
          'sources': [
            'crash_reporter/aw_microdump_crash_reporter_disabled_in_android_builds.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'libwebviewchromium',
      'includes': [
          'libwebviewchromium.gypi',
      ],
    }
  ],
  'conditions': [
    ['android_webview_build==0', {
      'includes': [
        'android_webview_tests.gypi',
      ],
      'targets': [
        {
          'target_name': 'android_webview_java',
          'type': 'none',
          'dependencies': [
            '../components/components.gyp:navigation_interception_java',
            '../components/components.gyp:web_contents_delegate_android_java',
            '../content/content.gyp:content_java',
            '../ui/android/ui_android.gyp:ui_java',
            'android_webview_strings_grd',
          ],
          'variables': {
            'java_in_dir': '../android_webview/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.android_webview',
            'R_package_relpath': 'org/chromium/android_webview',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'system_webview_apk',
          'variables': {
            'apk_name': 'SystemWebView',
            'android_sdk_jar': '../third_party/android_platform/webview/frameworks_5.1.0_r1.jar',
            'android_manifest_path': 'apk/java/AndroidManifest.xml',
            'java_in_dir': 'glue/java',
            'resource_dir': 'apk/java/res',
          },
          'includes': [ 'apk/system_webview_apk_common.gypi' ],
        },
      ],
     }, {  # android_webview_build==1
      'targets': [
        {
          'target_name': 'android_webview_jarjar_ui_resources',
          'android_unmangled_name': 1,
          'type': 'none',
          'variables': {
            'res_dir': '../ui/android/java/res',
            'rules_file': '../android_webview/build/jarjar-rules.txt',
          },
          'includes': ['../android_webview/build/jarjar_resources.gypi'],
        },
        {
          'target_name': 'android_webview_jarjar_content_resources',
          'android_unmangled_name': 1,
          'type': 'none',
          'variables': {
            'res_dir': '../content/public/android/java/res',
            'rules_file': '../android_webview/build/jarjar-rules.txt',
          },
          'includes': ['../android_webview/build/jarjar_resources.gypi'],
        },
        {
          'target_name': 'android_webview_jarjar_web_contents_delegate_android_resources',
          'android_unmangled_name': 1,
          'type': 'none',
          'variables': {
            'res_dir': '../components/web_contents_delegate_android/android/java/res',
            'rules_file': '../android_webview/build/jarjar-rules.txt',
          },
          'includes': ['../android_webview/build/jarjar_resources.gypi'],
        },
        {
          'target_name': 'android_webview_resources',
          'type': 'none',
          'android_unmangled_name': 1,
          'dependencies': [
            '../content/content.gyp:content_strings_grd',
            '../ui/android/ui_android.gyp:ui_strings_grd',
            'android_webview_jarjar_content_resources',
            'android_webview_jarjar_ui_resources',
            'android_webview_jarjar_web_contents_delegate_android_resources',
            'android_webview_strings_grd',
          ],
        },
      ],
    }],
  ],
}
