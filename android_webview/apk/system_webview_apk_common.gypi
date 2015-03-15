# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# When making changes remember that this is shared with the internal .apk
# build rules.
{
  'type': 'none',
  'dependencies': [
    '<(DEPTH)/android_webview/android_webview.gyp:libwebviewchromium',
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_java',
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_pak',
  ],
  'variables': {
    'native_lib_target': 'libwebviewchromium',
    'never_lint': 1,
    'R_package': 'com.android.webview.chromium',
    'R_package_relpath': 'com/android/webview/chromium',
    'shared_resources': 1,
    'extensions_to_not_compress': 'pak,bin,dat',
    'asset_location': '<(INTERMEDIATE_DIR)/assets/',
    'snapshot_copy_files': '<(snapshot_copy_files)',
    # TODO: crbug.com/442348 Update proguard.flags and re-enable.
    'proguard_enabled': 'false',
    'proguard_flags_paths': ['<(DEPTH)/android_webview/apk/java/proguard.flags'],
    # TODO: crbug.com/405035 Find a better solution for WebView .pak files.
    'additional_input_paths': [
      '<@(webview_locales_output_paks)',
      '<(asset_location)/webviewchromium.pak',
      '<(asset_location)/webview_licenses.notice',
      '<@(snapshot_additional_input_paths)',
    ],
    'includes': [
      '../snapshot_copying.gypi',
    ],
    'conditions': [
      ['icu_use_data_file_flag==1', {
        'additional_input_paths': [
          '<(asset_location)/icudtl.dat',
        ],
      }],
    ],
  },
  'copies': [
    {
      'destination': '<(asset_location)',
      'files': [
        '<@(webview_locales_input_paks)',
        '<(PRODUCT_DIR)/android_webview_assets/webviewchromium.pak',
        '<@(snapshot_copy_files)',
      ],
      'conditions': [
        ['icu_use_data_file_flag==1', {
          'files': [
            '<(PRODUCT_DIR)/icudtl.dat',
          ],
        }],
      ],
    },
  ],
  'actions': [
    {
      'action_name': 'generate_webview_license_notice',
      'inputs': [
        '<!@(python <(DEPTH)/android_webview/tools/webview_licenses.py notice_deps)',
        '<(DEPTH)/android_webview/tools/licenses_notice.tmpl',
        '<(DEPTH)/android_webview/tools/webview_licenses.py',
      ],
      'outputs': [
        '<(asset_location)/webview_licenses.notice',
      ],
      'action': [
        'python',
        '<(DEPTH)/android_webview/tools/webview_licenses.py',
        'notice',
        '<(asset_location)/webview_licenses.notice',
      ],
      'message': 'Generating WebView license notice',
    },
  ],
  'includes': [
    'system_webview_locales_paks.gypi',
    '../../build/java_apk.gypi',
  ],
}
