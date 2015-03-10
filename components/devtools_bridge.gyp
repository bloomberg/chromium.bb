# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_bridge_jni_headers',
      'type': 'none',
      'sources': [
        'devtools_bridge/android/java/src/org/chromium/components/devtools_bridge/SessionDependencyFactoryNative.java',
        'devtools_bridge/android/java/src/org/chromium/components/devtools_bridge/apiary/ApiaryClientFactory.java',
      ],
      'variables': {
        'jni_gen_package': 'devtools_bridge',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
    {
      'target_name': 'devtools_bridge',
      'type': 'static_library',
      'sources': [
        'devtools_bridge/android/apiary_client_factory.cc',
        'devtools_bridge/android/apiary_client_factory.h',
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
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../third_party/libjingle/libjingle.gyp:libjingle_webrtc',
        '../third_party/libjingle/libjingle.gyp:libpeerconnection',
        '../third_party/webrtc/base/base.gyp:rtc_base',
        'devtools_bridge_jni_headers',
      ],
    },
    {
      'target_name': 'devtools_bridge_javalib',
      'type': 'none',
      'variables': {
        'java_in_dir': 'devtools_bridge/android/java',
      },
      'includes': [ '../build/java.gypi' ],
      'dependencies': [
        '../base/base.gyp:base_java',
        '../sync/sync.gyp:sync_java',
        '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
      ],
    },
    {
      'target_name': 'libdevtools_bridge_natives_so',
      'type': 'shared_library',
      'sources': [
        'devtools_bridge/test/android/javatests/jni/jni_onload.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'devtools_bridge',
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
        'devtools_bridge_javalib',
      ],
    },
    {
      'target_name': 'devtools_bridge_tests_apk',
      'type': 'none',
      'dependencies': [
        'devtools_bridge_javalib',
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
  ],
}
