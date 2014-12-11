# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': 'none',
  'dependencies': [
    '<(DEPTH)/android_webview/android_webview.gyp:libwebviewchromium',
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_java',
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_pak',
  ],
  'variables': {
    'android_sdk_jar': '<(DEPTH)/third_party/android_platform/webview/frameworks_1622219.jar',
    'java_in_dir': 'java',
    'native_lib_target': 'libwebviewchromium',
    'never_lint': 1,
    'resource_dir': 'java/res',
    'R_package': 'com.android.webview.chromium',
    'R_package_relpath': 'com/android/webview/chromium',
    'extensions_to_not_compress': 'pak',
    'asset_location': '<(PRODUCT_DIR)/android_webview_assets',
    'proguard_enabled': 'true',
    'proguard_flags_paths': ['java/proguard.flags'],
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
