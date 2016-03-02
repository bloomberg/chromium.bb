# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/translate/core/browser
      'target_name': 'translate_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        '../url/url.gyp:url_lib',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        'data_use_measurement_core',
        'infobars_core',
        'language_usage_metrics',
        'pref_registry',
        'translate_core_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'translate/core/browser/language_state.cc',
        'translate/core/browser/language_state.h',
        'translate/core/browser/page_translated_details.h',
        'translate/core/browser/translate_accept_languages.cc',
        'translate/core/browser/translate_accept_languages.h',
        'translate/core/browser/translate_browser_metrics.cc',
        'translate/core/browser/translate_browser_metrics.h',
        'translate/core/browser/translate_client.h',
        'translate/core/browser/translate_download_manager.cc',
        'translate/core/browser/translate_download_manager.h',
        'translate/core/browser/translate_driver.h',
        'translate/core/browser/translate_error_details.h',
        'translate/core/browser/translate_event_details.cc',
        'translate/core/browser/translate_event_details.h',
        'translate/core/browser/translate_experiment.cc',
        'translate/core/browser/translate_experiment.h',
        'translate/core/browser/translate_language_list.cc',
        'translate/core/browser/translate_language_list.h',
        'translate/core/browser/translate_manager.cc',
        'translate/core/browser/translate_manager.h',
        'translate/core/browser/translate_prefs.cc',
        'translate/core/browser/translate_prefs.h',
        'translate/core/browser/translate_script.cc',
        'translate/core/browser/translate_script.h',
        'translate/core/browser/translate_step.h',
        'translate/core/browser/translate_ui_delegate.cc',
        'translate/core/browser/translate_ui_delegate.h',
        'translate/core/browser/translate_url_fetcher.cc',
        'translate/core/browser/translate_url_fetcher.h',
        'translate/core/browser/translate_url_util.cc',
        'translate/core/browser/translate_url_util.h',
      ],
      'conditions': [
        ['use_aura==0', {
          'sources': [
            'translate/core/browser/translate_infobar_delegate.cc',
            'translate/core/browser/translate_infobar_delegate.h',
          ],
        }],
        ['OS == "mac"', {
          'sources': [
            'translate/core/browser/options_menu_model.cc',
            'translate/core/browser/options_menu_model.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/translate/core/common
      'target_name': 'translate_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'translate/core/common/language_detection_details.cc',
        'translate/core/common/language_detection_details.h',
        'translate/core/common/translate_constants.cc',
        'translate/core/common/translate_constants.h',
        'translate/core/common/translate_errors.h',
        'translate/core/common/translate_metrics.cc',
        'translate/core/common/translate_metrics.h',
        'translate/core/common/translate_pref_names.cc',
        'translate/core/common/translate_pref_names.h',
        'translate/core/common/translate_switches.cc',
        'translate/core/common/translate_switches.h',
        'translate/core/common/translate_util.cc',
        'translate/core/common/translate_util.h',
      ],
    },
    {
      # GN version: //components/translate/core/language_detection
      'target_name': 'translate_core_language_detection',
      'type': 'static_library',
      'dependencies': [
        'translate_core_common',
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'translate/core/language_detection/language_detection_util.cc',
        'translate/core/language_detection/language_detection_util.h',
      ],
      'conditions': [
        ['cld_version==1', {
          'dependencies': [
            '<(DEPTH)/third_party/cld/cld.gyp:cld',
          ],
        }],
        ['cld_version==2', {
          'dependencies': [
            '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/translate/content/browser
          'target_name': 'translate_content_browser',
          'type': 'static_library',
          'dependencies': [
            'translate_content_common',
            'translate_core_browser',
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'translate/content/browser/browser_cld_data_provider.cc',
            'translate/content/browser/browser_cld_data_provider.h',
            'translate/content/browser/browser_cld_data_provider_factory.cc',
            'translate/content/browser/browser_cld_data_provider_factory.h',
            'translate/content/browser/browser_cld_utils.cc',
            'translate/content/browser/browser_cld_utils.h',
            'translate/content/browser/content_translate_driver.cc',
            'translate/content/browser/content_translate_driver.h',
            'translate/content/browser/data_file_browser_cld_data_provider.cc',
            'translate/content/browser/data_file_browser_cld_data_provider.h',
          ],
        },
        {
          # GN version: //components/translate/content/common
          'target_name': 'translate_content_common',
          'type': 'static_library',
          'dependencies': [
            'translate_core_common',
            'translate_core_language_detection',
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'translate/content/common/cld_data_source.cc',
            'translate/content/common/cld_data_source.h',
            'translate/content/common/data_file_cld_data_provider_messages.cc',
            'translate/content/common/data_file_cld_data_provider_messages.h',
            'translate/content/common/translate_messages.cc',
            'translate/content/common/translate_messages.h',
           ],
        },
        {
          # GN version: //components/translate/content/renderer
          'target_name': 'translate_content_renderer',
          'type': 'static_library',
          'dependencies': [
            'translate_content_common',
            'translate_core_common',
            'translate_core_language_detection',
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_renderer',
            '../ipc/ipc.gyp:ipc',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../url/url.gyp:url_lib',
            '../v8/tools/gyp/v8.gyp:v8',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'translate/content/renderer/renderer_cld_data_provider.cc',
            'translate/content/renderer/renderer_cld_data_provider.h',
            'translate/content/renderer/renderer_cld_data_provider_factory.cc',
            'translate/content/renderer/renderer_cld_data_provider_factory.h',
            'translate/content/renderer/renderer_cld_utils.cc',
            'translate/content/renderer/renderer_cld_utils.h',
            'translate/content/renderer/translate_helper.cc',
            'translate/content/renderer/translate_helper.h',
           ],
          'conditions': [
            ['cld_version==2', {
              'dependencies': [
                '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
              ],
              'sources': [
                'translate/content/renderer/data_file_renderer_cld_data_provider.cc',
                'translate/content/renderer/data_file_renderer_cld_data_provider.h',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS == "ios"', {
      'targets': [
        {
          # GN version: //components/translate/ios/browser
          'target_name': 'translate_ios_browser',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'translate_core_language_detection',
            'translate_core_browser',
            'translate_core_common',
            'translate_ios_injected_js',
            '../base/base.gyp:base',
            '../ios/web/ios_web.gyp:ios_web',
            '../url/url.gyp:url_lib',
          ],
          'sources': [
            'translate/ios/browser/ios_translate_driver.h',
            'translate/ios/browser/ios_translate_driver.mm',
            'translate/ios/browser/js_language_detection_manager.h',
            'translate/ios/browser/js_language_detection_manager.mm',
            'translate/ios/browser/js_translate_manager.h',
            'translate/ios/browser/js_translate_manager.mm',
            'translate/ios/browser/language_detection_controller.h',
            'translate/ios/browser/language_detection_controller.mm',
            'translate/ios/browser/translate_controller.h',
            'translate/ios/browser/translate_controller.mm',
          ],
        },
        {
          # GN version: //components/translate/ios/browser:injected_js
          'target_name': 'translate_ios_injected_js',
          'type': 'none',
          'sources': [
            'translate/ios/browser/resources/language_detection.js',
            'translate/ios/browser/resources/translate_ios.js',
          ],
          'link_settings': {
            'mac_bundle_resources': [
              '<(SHARED_INTERMEDIATE_DIR)/translate_ios.js',
              '<(SHARED_INTERMEDIATE_DIR)/language_detection.js',
            ],
          },
          'includes': [
            '../ios/web/js_compile_checked.gypi',
          ],
        },
      ],
    }],
  ],
}
