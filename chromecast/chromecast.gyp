# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'android_support_v13_target%':
        '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
    'chromium_code': 1,
    'chromecast_branding%': 'Chromium',
  },
  'target_defaults': {
    'include_dirs': [
      '..',  # Root of Chromium checkout
    ],
  },
  'targets': [
    {
      'target_name': 'cast_base',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'base/metrics/cast_histograms.h',
        'base/metrics/cast_metrics_helper.cc',
        'base/metrics/cast_metrics_helper.h',
      ],
    },  # end of target 'cast_base'
    {
      'target_name': 'cast_net',
      'type': '<(component)',
      'sources': [
        'net/network_change_notifier_cast.cc',
        'net/network_change_notifier_cast.h',
        'net/network_change_notifier_factory_cast.cc',
        'net/network_change_notifier_factory_cast.h',
      ],
    },
    {
      'target_name': 'cast_shell_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast',
      },
      'actions': [
        {
          'action_name': 'cast_shell_resources',
          'variables': {
            'grit_grd_file': 'app/resources/shell_resources.grd',
            'grit_resource_ids': 'app/resources/resource_ids',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'cast_shell_pak',
      'type': 'none',
      'dependencies': [
        'cast_shell_resources',
        '../content/app/resources/content_resources.gyp:content_resources',
        '../content/app/strings/content_strings.gyp:content_strings',
        '../net/net.gyp:net_resources',
        '../third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '../ui/resources/ui_resources.gyp:ui_resources',
        '../ui/strings/ui_strings.gyp:ui_strings',
      ],
      'actions': [
        {
          'action_name': 'repack_cast_shell_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chromecast/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/assets/cast_shell.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
    },
    # This target contains all content-embedder implementation that is
    # non-platform-specific.
    {
      'target_name': 'cast_shell_common',
      'type': '<(component)',
      'dependencies': [
        'cast_base',
        'cast_shell_pak',
        'cast_shell_resources',
        'cast_version_header',
        'chromecast_locales.gyp:chromecast_locales_pak',
        'chromecast_locales.gyp:chromecast_settings',
        'media/media.gyp:media_base',
        '../base/base.gyp:base',
        '../components/components.gyp:cdm_renderer',
        '../components/components.gyp:component_metrics_proto',
        '../components/components.gyp:metrics',
        '../components/components.gyp:metrics_gpu',
        '../components/components.gyp:metrics_net',
        '../components/components.gyp:metrics_profiler',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_browser',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'app/cast_main_delegate.cc',
        'app/cast_main_delegate.h',
        'browser/cast_browser_context.cc',
        'browser/cast_browser_context.h',
        'browser/cast_browser_main_parts.cc',
        'browser/cast_browser_main_parts.h',
        'browser/cast_browser_process.cc',
        'browser/cast_browser_process.h',
        'browser/cast_content_browser_client.cc',
        'browser/cast_content_browser_client.h',
        'browser/cast_content_window.cc',
        'browser/cast_content_window.h',
        'browser/cast_download_manager_delegate.cc',
        'browser/cast_download_manager_delegate.h',
        'browser/cast_http_user_agent_settings.cc',
        'browser/cast_http_user_agent_settings.h',
        'browser/cast_network_delegate.cc',
        'browser/cast_network_delegate.h',
        'browser/devtools/cast_dev_tools_delegate.cc',
        'browser/devtools/cast_dev_tools_delegate.h',
        'browser/devtools/remote_debugging_server.cc',
        'browser/devtools/remote_debugging_server.h',
        'browser/geolocation/cast_access_token_store.cc',
        'browser/geolocation/cast_access_token_store.h',
        'browser/metrics/cast_metrics_prefs.cc',
        'browser/metrics/cast_metrics_prefs.h',
        'browser/metrics/cast_metrics_service_client.cc',
        'browser/metrics/cast_metrics_service_client.h',
        'browser/metrics/cast_stability_metrics_provider.cc',
        'browser/metrics/cast_stability_metrics_provider.h',
        'browser/metrics/platform_metrics_providers.h',
        'browser/service/cast_service.cc',
        'browser/service/cast_service.h',
        'browser/url_request_context_factory.cc',
        'browser/url_request_context_factory.h',
        'browser/webui/webui_cast.h',
        'common/cast_content_client.cc',
        'common/cast_content_client.h',
        'common/cast_paths.cc',
        'common/cast_paths.h',
        'common/cast_resource_delegate.cc',
        'common/cast_resource_delegate.h',
        'common/chromecast_config.cc',
        'common/chromecast_config.h',
        'common/chromecast_switches.cc',
        'common/chromecast_switches.h',
        'common/platform_client_auth.h',
        'common/pref_names.cc',
        'common/pref_names.h',
        'renderer/cast_content_renderer_client.cc',
        'renderer/cast_content_renderer_client.h',
        'renderer/key_systems_cast.cc',
        'renderer/key_systems_cast.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            '<(cast_internal_gyp):cast_shell_internal',
          ],
        }, {
          'sources': [
            'browser/cast_network_delegate_simple.cc',
            'browser/devtools/remote_debugging_server_simple.cc',
            'browser/metrics/platform_metrics_providers_simple.cc',
            'browser/webui/webui_cast_simple.cc',
            'common/chromecast_config_simple.cc',
            'common/platform_client_auth_simple.cc',
            'renderer/key_systems_cast_simple.cc',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'browser/service/cast_service_android.cc',
                'browser/service/cast_service_android.h',
              ],
            }, {
              'sources': [
                'browser/service/cast_service_simple.cc',
                'browser/service/cast_service_simple.h',
              ],
            }],
          ],
        }],
        # ExternalMetrics not necessary on Android and (as of this writing) uses
        # non-portable filesystem operations. Also webcrypto is not used on
        # Android either.
        ['OS=="linux"', {
          'sources': [
            'browser/metrics/external_metrics.cc',
            'browser/metrics/external_metrics.h',
          ],
          'dependencies': [
            '../components/components.gyp:metrics_serialization',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_shell_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_shell_common',
        '../base/base.gyp:base_prefs_test_support',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:component_metrics_proto',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'browser/metrics/cast_metrics_service_client_unittest.cc',
      ],
      'conditions': [
        ['use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ]
    },  # end of target 'cast_metrics_unittests'
    {
      'target_name': 'cast_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'inputs': [
            '<(version_path)',
            'common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chromecast/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            # Revision is taken from buildbot if available; otherwise, a dev string is used.
            '-e', 'CAST_BUILD_REVISION="<!(echo ${BUILD_NUMBER:="local.${USER}"})"',
            '-e', 'CAST_IS_DEBUG_BUILD=1 if "<(CONFIGURATION_NAME)" == "Debug" else 0',
            'common/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../build/util/version.gypi',
          ],
        },
      ],
    },
    {
      'target_name': 'cast_metrics_test_support',
      'type': '<(component)',
      'dependencies': [
        'cast_base',
      ],
      'sources': [
        'base/metrics/cast_metrics_test_helper.cc',
        'base/metrics/cast_metrics_test_helper.h',
      ],
    },  # end of target 'cast_metrics_test_support'
    {
      'target_name': 'cast_tests',
      'type': 'none',
      'dependencies': [
        'media/media.gyp:cast_media_unittests',
      ],
    },
  ],  # end of targets

  # Targets for Android receiver.
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'libcast_shell_android',
          'type': 'shared_library',
          'dependencies': [
            'cast_jni_headers',
            'cast_shell_common',
            'cast_shell_pak',
            'cast_version_header',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_client',
            '../components/components.gyp:breakpad_host',
            '../components/components.gyp:crash_component',
            '../content/content.gyp:content_app_browser',
            '../content/content.gyp:content',
            '../skia/skia.gyp:skia',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gl/gl.gyp:gl',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'sources': [
            'android/cast_jni_registrar.cc',
            'android/cast_jni_registrar.h',
            'android/chromecast_config_android.cc',
            'android/chromecast_config_android.h',
            'android/platform_jni_loader.h',
            'app/android/cast_jni_loader.cc',
            'browser/android/cast_window_android.cc',
            'browser/android/cast_window_android.h',
            'browser/android/cast_window_manager.cc',
            'browser/android/cast_window_manager.h',
            'browser/android/external_video_surface_container_impl.cc',
            'browser/android/external_video_surface_container_impl.h',
            'crash/android/cast_crash_reporter_client_android.cc',
            'crash/android/cast_crash_reporter_client_android.h',
            'crash/android/crash_handler.cc',
            'crash/android/crash_handler.h',
          ],
          'conditions': [
            ['chromecast_branding=="Chrome"', {
              'dependencies': [
                '<(cast_internal_gyp):cast_shell_android_internal'
              ],
            }, {
              'sources': [
                'android/chromecast_config_android_stub.cc',
                'android/platform_jni_loader_stub.cc',
              ],
            }]
          ],
        },  # end of target 'libcast_shell_android'
        {
          'target_name': 'cast_shell_java',
          'type': 'none',
          'dependencies': [
            '<(android_support_v13_target)',
            '../base/base.gyp:base_java',
            '../content/content.gyp:content_java',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net_java',
            '../ui/android/ui_android.gyp:ui_java',
          ],
          'variables': {
            'has_java_resources': 1,
            'java_in_dir': 'browser/android/apk',
            'resource_dir': 'browser/android/apk/res',
            'R_package': 'org.chromium.chromecast.shell',
          },
          'includes': ['../build/java.gypi'],
        },  # end of target 'cast_shell_java'
        {
          'target_name': 'cast_shell_apk',
          'type': 'none',
          'dependencies': [
            'cast_shell_java',
            'libcast_shell_android',
          ],
          'variables': {
            'apk_name': 'CastShell',
            'manifest_package_name': 'org.chromium.chromecast.shell',
            # Note(gunsch): there are no Java files in the android/ directory.
            # Unfortunately, the java_apk.gypi target rigidly insists on having
            # a java_in_dir directory, but complains about duplicate classes
            # from the common cast_shell_java target (shared with internal APK)
            # if the actual Java path is used.
            # This will hopefully be removable after the great GN migration.
            'java_in_dir': 'android',
            'android_manifest_path': 'browser/android/apk/AndroidManifest.xml',
            'package_name': 'org.chromium.chromecast.shell',
            'native_lib_target': 'libcast_shell_android',
            'asset_location': '<(PRODUCT_DIR)/assets',
            'additional_input_paths': ['<(PRODUCT_DIR)/assets/cast_shell.pak'],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          'target_name': 'cast_jni_headers',
          'type': 'none',
          'sources': [
            'browser/android/apk/src/org/chromium/chromecast/shell/CastCrashHandler.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastWindowAndroid.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastWindowManager.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/ExternalVideoSurfaceContainer.java',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/chromecast',
            ],
          },
          'variables': {
            'jni_gen_package': 'chromecast',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],  # end of targets
    }, {  # OS != "android"
      'targets': [
        # This target contains all of the primary code of |cast_shell|, except
        # for |main|. This allows end-to-end tests using |cast_shell|.
        # This also includes all targets that cannot be built on Android.
        {
          'target_name': 'cast_shell_core',
          'type': '<(component)',
          'dependencies': [
            'cast_net',
            'cast_shell_common',
            'media/media.gyp:cast_media',
            '../ui/aura/aura.gyp:aura_test_support',
          ],
          'conditions': [
            ['chromecast_branding=="Chrome"', {
              'dependencies': [
                '<(cast_internal_gyp):cast_gfx_internal',
              ],
            }, {
              'dependencies': [
                '../ui/ozone/ozone.gyp:eglplatform_shim_x11',
              ],
            }],
          ],
        },
        {
          'target_name': 'cast_shell',
          'type': 'executable',
          'dependencies': [
            'cast_shell_core',
          ],
          'sources': [
            'app/cast_main.cc',
          ],
        },
        {
          'target_name': 'cast_shell_browser_test',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_shell_test_support',
            '../testing/gtest.gyp:gtest',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'browser/test/chromecast_shell_browser_test.cc',
          ],
        },
        {
          'target_name': 'cast_shell_test_support',
          'type': '<(component)',
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'dependencies': [
            'cast_shell_core',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'browser/test/chromecast_browser_test.cc',
            'browser/test/chromecast_browser_test.h',
            'browser/test/chromecast_browser_test_runner.cc',
          ],
        },
      ],  # end of targets
    }],
  ],  # end of conditions
}
