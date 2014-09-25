# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
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
      'target_name': 'cast_common',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'common/cast_paths.cc',
        'common/cast_paths.h',
        'common/cast_resource_delegate.cc',
        'common/cast_resource_delegate.h',
        'common/chromecast_config.cc',
        'common/chromecast_config.h',
        'common/chromecast_switches.cc',
        'common/chromecast_switches.h',
        'common/pref_names.cc',
        'common/pref_names.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:cast_common_internal',
          ],
        }, {
          'sources': [
            'common/chromecast_config_simple.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_metrics',
      'type': '<(component)',
      'dependencies': [
        'cast_common',
        '../components/components.gyp:component_metrics_proto',
        '../components/components.gyp:metrics',
        '../components/components.gyp:metrics_gpu',
        '../components/components.gyp:metrics_net',
        '../components/components.gyp:metrics_profiler',
      ],
      'sources': [
        'metrics/cast_metrics_prefs.cc',
        'metrics/cast_metrics_prefs.h',
        'metrics/cast_metrics_service_client.cc',
        'metrics/cast_metrics_service_client.h',
        'metrics/platform_metrics_providers.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            '<(cast_internal_gyp):cast_metrics_internal',
          ],
        }, {
          'sources': [
            'metrics/platform_metrics_providers_simple.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_metrics_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_metrics',
        '../base/base.gyp:base_prefs_test_support',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:component_metrics_proto',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'metrics/cast_metrics_service_client_unittest.cc',
      ],
    },  # end of target 'cast_metrics_unittests'
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
      'target_name': 'cast_service',
      'type': '<(component)',
      'dependencies': [
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'service/cast_service.cc',
        'service/cast_service.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:cast_service_internal',
          ],
        }, {
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'service/cast_service_android.cc',
                'service/cast_service_android.h',
              ],
            }, {
              'sources': [
                'service/cast_service_simple.cc',
                'service/cast_service_simple.h',
              ],
            }],
          ],
        }],
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
            'grit_grd_file': 'shell/browser/resources/shell_resources.grd',
            'grit_resource_ids': 'shell/browser/resources/resource_ids',
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
        '../content/browser/devtools/devtools_resources.gyp:devtools_resources',
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
              '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
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
        'cast_common',
        'cast_metrics',
        'cast_service',
        'cast_shell_pak',
        'cast_shell_resources',
        'cast_version_header',
        'chromecast_locales.gyp:chromecast_locales_pak',
        'chromecast_locales.gyp:chromecast_settings',
        'media/media.gyp:media_base',
        '../components/components.gyp:cdm_renderer',
        '../components/components.gyp:component_metrics_proto',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_browser',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'shell/app/cast_main_delegate.cc',
        'shell/app/cast_main_delegate.h',
        'shell/browser/cast_browser_context.cc',
        'shell/browser/cast_browser_context.h',
        'shell/browser/cast_browser_main_parts.cc',
        'shell/browser/cast_browser_main_parts.h',
        'shell/browser/cast_browser_process.cc',
        'shell/browser/cast_browser_process.h',
        'shell/browser/cast_content_browser_client.cc',
        'shell/browser/cast_content_browser_client.h',
        'shell/browser/cast_download_manager_delegate.cc',
        'shell/browser/cast_download_manager_delegate.h',
        'shell/browser/cast_http_user_agent_settings.cc',
        'shell/browser/cast_http_user_agent_settings.h',
        'shell/browser/devtools/cast_dev_tools_delegate.cc',
        'shell/browser/devtools/cast_dev_tools_delegate.h',
        'shell/browser/devtools/remote_debugging_server.cc',
        'shell/browser/devtools/remote_debugging_server.h',
        'shell/browser/geolocation/cast_access_token_store.cc',
        'shell/browser/geolocation/cast_access_token_store.h',
        'shell/browser/url_request_context_factory.cc',
        'shell/browser/url_request_context_factory.h',
        'shell/browser/webui/webui_cast.h',
        'shell/common/cast_content_client.cc',
        'shell/common/cast_content_client.h',
        'shell/renderer/cast_content_renderer_client.cc',
        'shell/renderer/cast_content_renderer_client.h',
        'shell/renderer/key_systems_cast.cc',
        'shell/renderer/key_systems_cast.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:cast_shell_internal',
          ],
        }, {
          'sources': [
            'shell/browser/devtools/remote_debugging_server_simple.cc',
            'shell/browser/webui/webui_cast_simple.cc',
            'shell/renderer/key_systems_cast_simple.cc',
          ],
        }],
      ],
    },
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
            'cast_common',
            'cast_jni_headers',
            'cast_shell_common',
            'cast_shell_pak',
            'cast_version_header',
            '../base/base.gyp:base',
            '../content/content.gyp:content_app_browser',
            '../content/content.gyp:content',
            '../skia/skia.gyp:skia',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gl/gl.gyp:gl',
          ],
          'sources': [
            'android/cast_jni_registrar.cc',
            'android/cast_jni_registrar.h',
            'android/chromecast_config_android.cc',
            'android/chromecast_config_android.h',
            'android/platform_jni_loader.h',
            'shell/app/android/cast_jni_loader.cc',
            'shell/browser/android/cast_window_manager.cc',
            'shell/browser/android/cast_window_manager.h',
            'shell/browser/android/cast_window_android.cc',
            'shell/browser/android/cast_window_android.h',
          ],
          'conditions': [
            ['chromecast_branding=="Chrome"', {
              'dependencies': [
                '<(cast_internal_gyp):cast_shell_android_internal'
              ],
            }, {
              'sources': [
                'android/platform_jni_loader_stub.cc',
              ],
            }]
          ],
        },  # end of target 'libcast_shell_android'
        {
          'target_name': 'cast_shell_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../content/content.gyp:content_java',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net_java',
            '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
            '../ui/android/ui_android.gyp:ui_java',
          ],
          'variables': {
            'has_java_resources': 1,
            'java_in_dir': 'shell/android/apk',
            'resource_dir': 'shell/android/apk/res',
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
            'android_manifest_path': 'shell/android/apk/AndroidManifest.xml',
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
            'shell/android/apk/src/org/chromium/chromecast/shell/CastWindowAndroid.java',
            'shell/android/apk/src/org/chromium/chromecast/shell/CastWindowManager.java',
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
                'internal/chromecast_internal.gyp:cast_gfx_internal',
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
            'shell/app/cast_main.cc',
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
            'shell/browser/test/chromecast_shell_browser_test.cc',
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
            'shell/browser/test/chromecast_browser_test.cc',
            'shell/browser/test/chromecast_browser_test.h',
            'shell/browser/test/chromecast_browser_test_runner.cc',
          ],
        },
      ],  # end of targets
    }],
  ],  # end of conditions
}
