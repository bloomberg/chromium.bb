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
    'extensions_to_not_compress': 'pak',
    'asset_location': '<(PRODUCT_DIR)/android_webview_assets',
    # TODO: crbug.com/442348 Update proguard.flags and re-enable.
    'proguard_enabled': 'false',
    'proguard_flags_paths': ['<(DEPTH)/android_webview/apk/java/proguard.flags'],
    # TODO: crbug.com/405035 Find a better solution for WebView .pak files.
    'additional_input_paths': [
      '<(PRODUCT_DIR)/android_webview_assets/webviewchromium.pak',
      '<(PRODUCT_DIR)/android_webview_assets/en-US.pak',
    ],
    'conditions': [
      ['icu_use_data_file_flag==1', {
        'additional_input_paths': [
          '<(PRODUCT_DIR)/icudtl.dat',
        ],
      }],
      ['v8_use_external_startup_data==1', {
        'additional_input_paths': [
          '<(PRODUCT_DIR)/natives_blob.bin',
          '<(PRODUCT_DIR)/snapshot_blob.bin',
        ],
      }],
    ],
  },
  'includes': [ '../../build/java_apk.gypi' ],
}
