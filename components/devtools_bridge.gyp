# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/util/version.gypi',
  ],
  'targets': [
    {
      'target_name': 'devtools_bridge_jni_headers',
      'type': 'none',
      'sources': [
        'devtools_bridge/android/java/src/org/chromium/components/devtools_bridge/SessionDependencyFactoryNative.java',
        'devtools_bridge/test/android/client/javatests/src/org/chromium/components/devtools_bridge/WebClient.java',
      ],
      'variables': {
        'jni_gen_package': 'devtools_bridge',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
    {
      'target_name': 'devtools_bridge_server',
      'type': 'static_library',
      'sources': [
        'devtools_bridge/android/session_dependency_factory_android.cc',
        'devtools_bridge/android/session_dependency_factory_android.h',
        'devtools_bridge/session_dependency_factory.cc',
        'devtools_bridge/session_dependency_factory.h',
        'devtools_bridge/socket_tunnel_connection.cc',
        'devtools_bridge/socket_tunnel_connection.h',
        'devtools_bridge/socket_tunnel_packet_handler.cc',
        'devtools_bridge/socket_tunnel_packet_handler.h',
        'devtools_bridge/socket_tunnel_server.cc',
        'devtools_bridge/socket_tunnel_server.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libjingle_webrtc',
        '<(DEPTH)/third_party/libjingle/libjingle.gyp:libpeerconnection',
        '<(DEPTH)/third_party/webrtc/base/base.gyp:webrtc_base',
        'devtools_bridge_jni_headers',
      ],
    },
    {
      'target_name': 'devtools_bridge_server_javalib',
      'type': 'none',
      'variables': {
        'java_in_dir': 'devtools_bridge/android/java',
      },
      'includes': [ '../build/java.gypi' ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_java',
        '<(DEPTH)/third_party/android_tools/android_tools.gyp:android_gcm',
      ],
    },
    {
      'target_name': 'libdevtools_bridge_natives_so',
      'type': 'shared_library',
      'sources': [
        'devtools_bridge/test/android/javatests/jni/jni_onload.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'devtools_bridge_server',
      ],
    },
    {
      'target_name': 'devtools_bridge_testutils',
      'type': 'none',
      'variables': {
        'java_in_dir': 'devtools_bridge/test/android/javatests',
      },
      'includes': [ '../build/java.gypi' ],
      'dependencies': [
        'devtools_bridge_server_javalib',
      ],
    },
    {
      'target_name': 'devtools_bridge_tests_apk',
      'type': 'none',
      'dependencies': [
        'devtools_bridge_server_javalib',
        'devtools_bridge_testutils',
        'libdevtools_bridge_natives_so',
      ],
      'variables': {
        'apk_name': 'DevToolsBridgeTest',
        'test_suite_name': 'devtools_bridge_tests',
        'java_in_dir': 'devtools_bridge/android/javatests',
        'native_lib_target': 'libdevtools_bridge_natives_so',
        'is_test_apk': 1,
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
    {
      'target_name': 'libdevtools_bridge_browsertests',
      'type': 'shared_library',
      'sources': [
        'devtools_bridge/test/android/client/javatests/jni/jni_onload.cc',
        'devtools_bridge/test/android/client/web_client_android.cc',
        'devtools_bridge/test/android/client/web_client_android.h',
      ],
      'dependencies': [
        '<(DEPTH)/chrome/chrome.gyp:libchromeshell_base',
        'devtools_bridge_client',
        'devtools_bridge_jni_headers',
        'devtools_bridge_server',
      ],
    },
    {
      'target_name': 'devtools_bridge_browsertests_resources',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
      ],
      'variables': {
        'asset_location': '<(PRODUCT_DIR)/devtools_bridge_browsertests_apk/assets',
      },
      'inputs': [
        '<(PRODUCT_DIR)/chrome_100_percent.pak',
        '<(PRODUCT_DIR)/locales/en-US.pak',
        '<(PRODUCT_DIR)/resources.pak',
      ],
      'copies': [
        {
          'destination': '<(asset_location)',
          'files': [
            '<(PRODUCT_DIR)/chrome_100_percent.pak',
            '<(PRODUCT_DIR)/locales/en-US.pak',
            '<(PRODUCT_DIR)/resources.pak',
          ],
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'files': [ '<(PRODUCT_DIR)/icudtl.dat' ],
            }],
            ['v8_use_external_startup_data==1', {
              'files': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        },
      ],
    },
    {
      'target_name': 'devtools_bridge_browsertests_apk',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_java',
        '<(DEPTH)/chrome/chrome.gyp:chrome_java',
        'devtools_bridge_browsertests_resources',
        'devtools_bridge_server_javalib',
        'libdevtools_bridge_browsertests',
      ],
      'variables': {
        'apk_name': 'DevToolsBridgeBrowserTests',
        'test_suite_name': 'devtools_bridge_tests',
        'java_in_dir': 'devtools_bridge/android/client/javatests',
        'additional_src_dirs': ['devtools_bridge/test/android/client/javatests'],
        'native_lib_target': 'libdevtools_bridge_browsertests',
        'asset_location': '<(PRODUCT_DIR)/devtools_bridge_browsertests_apk/assets',
        'native_lib_version_name': '<(version_full)',
        'is_test_apk': 1,
      },
      'includes': [ '../build/java_apk.gypi' ],
    },

    # TODO(serya): Separate from android targets. Otherwise it may not be
    # used outside of android.
    {
      'target_name': 'devtools_bridge_client',
      'type': 'static_library',
      'sources': [
        'devtools_bridge/client/web_client.cc',
        'devtools_bridge/client/web_client.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content',
      ],
    },
  ],
}
