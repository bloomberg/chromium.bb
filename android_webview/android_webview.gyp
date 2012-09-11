# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'libwebview',
      'type': 'shared_library',
      'dependencies': [
        '<(DEPTH)/chrome/chrome.gyp:browser',
        '<(DEPTH)/chrome/chrome.gyp:renderer',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/android_webview/native/webview_native.gyp:webview_native',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android',
        '<(DEPTH)/chrome/browser/component/components.gyp:browser_component_jni_headers',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/skia/config',
      ],
      'sources': [
        'common/android_webview_message_generator.cc',
        'common/android_webview_message_generator.h',
        'common/render_view_messages.cc',
        'common/render_view_messages.h',
        'browser/net/aw_network_delegate.cc',
        'browser/net/aw_network_delegate.h',
        'browser/renderer_host/aw_render_view_host_ext.cc',
        'browser/renderer_host/aw_render_view_host_ext.h',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.cc',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.h',
        'browser/aw_cookie_access_policy.cc',
        'browser/aw_cookie_access_policy.h',
        'lib/aw_browser_dependency_factory_impl.cc',
        'lib/aw_browser_dependency_factory_impl.h',
        'lib/aw_content_browser_client.cc',
        'lib/aw_content_browser_client.h',
        'lib/main/aw_main_delegate.cc',
        'lib/main/aw_main_delegate.h',
        'lib/main/webview_entry_point.cc',
        'lib/main/webview_stubs.cc',
        'renderer/aw_render_view_ext.cc',
        'renderer/aw_render_view_ext.h',
      ],
    },
    {
      'target_name': 'android_webview',
      'type' : 'none',
      'dependencies': [
        'libwebview',
      ],
      'variables': {
        'install_binary_script': 'build/install_binary',
      },
      'actions': [
        {
          'action_name': 'libwebview_strip_and_install_in_android',
          'inputs': [
            '<(SHARED_LIB_DIR)/libwebview.so',
          ],
          'outputs': [
            '<(android_product_out)/obj/lib/libwebview.so',
            '<(android_product_out)/system/lib/libwebview.so',
            '<(android_product_out)/symbols/system/lib/libwebview.so',
            '<(android_product_out)/symbols/data/data/org.chromium.android_webview/lib/libwebview.so',
          ],
          'action': [
            '<(install_binary_script)',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'android_webview_java',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/content/content.gyp:content_java',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android_java',
      ],
      'variables': {
        'package_name': 'android_webview_java',
        'java_in_dir': '<(DEPTH)/android_webview/java',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'android_webview_javatests',
      'type': 'none',
      'dependencies': [
        'android_webview_java',
        '<(DEPTH)/base/base.gyp:base_java_test_support',
        '<(DEPTH)/content/content.gyp:content_java',
        '<(DEPTH)/content/content.gyp:content_javatests',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android_java',
      ],
      'variables': {
        'package_name': 'android_webview_javatests',
        'java_in_dir': '<(DEPTH)/android_webview/javatests',
      },
      'includes': [ '../build/java.gypi' ],
    },

    {
      'target_name': 'android_webview_apk',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview/assets',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/en-US.pak',
          ]
        },
      ],
      'dependencies': [
        'libwebview',
        '../base/base.gyp:base_java',
        '../net/net.gyp:net_java',
        '../media/media.gyp:media_java',
        # TODO: This should be removed once we stop sharing the chrome/ layer JNI
        # registration code.  We currently include this because we reuse the
        # chrome/ layer JNI registration code (which will crash if these classes
        # are not present in the APK).
        '../chrome/chrome.gyp:chrome_java',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android_java',
        '../content/content.gyp:content_java',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
      ],
      'actions': [
        {
          'action_name': 'copy_and_strip_so',
          'inputs': ['<(SHARED_LIB_DIR)/libwebview.so'],
          'outputs': ['<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so'],
          'action': [
            '<!(/bin/echo -n $STRIP)',
            '--strip-unneeded',  # All symbols not needed for relocation.
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
        {
          'action_name': 'android_webview_apk',
          'inputs': [
            '../build/android/ant/common.xml',
            '../build/android/ant/sdk-targets.xml',
            '<(DEPTH)/android_webview/java/android_webview_apk.xml',
            '<(DEPTH)/android_webview/java/AndroidManifest.xml',
            '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
            '<(SHARED_INTERMEDIATE_DIR)/repack/en-US.pak',
            '<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so',
            '>@(input_jars_paths)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/android_webview/AndroidWebView-debug.apk',
          ],
          'action': [
            'ant',
            '-DPRODUCT_DIR=<(ant_build_out)',
            '-DAPP_ABI=<(android_app_abi)',
            '-DANDROID_SDK=<(android_sdk)',
            '-DANDROID_SDK_ROOT=<(android_sdk_root)',
            '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
            '-DANDROID_SDK_VERSION=<(android_sdk_version)',
            '-DANDROID_GDBSERVER=<(android_gdbserver)',
            '-DANDROID_TOOLCHAIN=<(android_toolchain)',
            '-DINPUT_JARS_PATHS=>(input_jars_paths)',
            '-buildfile',
            '<(DEPTH)/android_webview/java/android_webview_apk.xml',
          ],
        }
      ],
    },
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_apk',
        'android_webview_java',
        '../base/base.gyp:base_java',
        '../base/base.gyp:base_java_test_support',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android_java',
        '../content/content.gyp:content_java',
        '../content/content.gyp:content_javatests',
        '../media/media.gyp:media_java',
        '../net/net.gyp:net_java',
        '<(DEPTH)/content/content.gyp:content_javatests',
      ],
      'actions': [
        {
          'action_name': 'android_webview_test_generate_apk',
          'inputs': [
            '../build/android/ant/common.xml',
            '../build/android/ant/sdk-targets.xml',
            '<(DEPTH)/android_webview/javatests/android_webview_test_apk.xml',
            '<(DEPTH)/android_webview/javatests/AndroidManifest.xml',
            '<!@(find <(DEPTH)/android_webview/javatests/ -name "*.java")',
            '>@(input_jars_paths)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/android_webview_test/AndroidWebViewTest-debug.apk',
          ],
          'action': [
            'ant',
            '-DPRODUCT_DIR=<(ant_build_out)',
            '-DAPP_ABI=<(android_app_abi)',
            '-DANDROID_SDK=<(android_sdk)',
            '-DANDROID_SDK_ROOT=<(android_sdk_root)',
            '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
            '-DANDROID_SDK_VERSION=<(android_sdk_version)',
            '-DANDROID_GDBSERVER=<(android_gdbserver)',
            '-DINPUT_JARS_PATHS=>(input_jars_paths)',
            '-buildfile',
            '<(DEPTH)/android_webview/javatests/android_webview_test_apk.xml',
          ]
        }
      ],
    },
  ],
}
