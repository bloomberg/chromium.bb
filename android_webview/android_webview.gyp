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
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:renderer',
        '../content/content.gyp:content',
        '../android_webview/native/webview_native.gyp:webview_native',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android',
        '../chrome/browser/component/components.gyp:browser_component_jni_headers',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
      ],
      'sources': [
        'common/android_webview_message_generator.cc',
        'common/android_webview_message_generator.h',
        'common/render_view_messages.cc',
        'common/render_view_messages.h',
        'common/url_constants.cc',
        'common/url_constants.h',
        'browser/aw_cookie_access_policy.cc',
        'browser/aw_cookie_access_policy.h',
        'browser/aw_http_auth_handler_base.cc',
        'browser/aw_http_auth_handler_base.h',
        'browser/aw_login_delegate.cc',
        'browser/aw_login_delegate.h',
        'browser/find_helper.cc',
        'browser/find_helper.h',
        'browser/net/aw_network_delegate.cc',
        'browser/net/aw_network_delegate.h',
        'browser/renderer_host/aw_render_view_host_ext.cc',
        'browser/renderer_host/aw_render_view_host_ext.h',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.cc',
        'browser/renderer_host/aw_resource_dispatcher_host_delegate.h',
        'browser/scoped_allow_wait_for_legacy_web_view_api.h',
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
        '../content/content.gyp:content_java',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android_java',
        '../ui/ui.gyp:ui_java',
      ],
      'variables': {
        'package_name': 'android_webview_java',
        'java_in_dir': '../android_webview/java',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'android_webview_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android_java',
        '../chrome/chrome_resources.gyp:packed_extra_resources',
        '../chrome/chrome_resources.gyp:packed_resources',
        '../content/content.gyp:content_java',
        '../media/media.gyp:media_java',
        '../net/net.gyp:net_java',
        '../ui/ui.gyp:ui_java',
        'libwebview',
      ],
      'variables': {
        'package_name': 'android_webview',
        'apk_name': 'AndroidWebView',
        'java_in_dir': '../android_webview/java',
        'resource_dir': '../res',
        'native_libs_paths': ['<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so'],
        'input_pak_files': [
          '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/en-US.pak',
        ],
        'copied_pak_files': [
          '<(PRODUCT_DIR)/android_webview/assets/chrome.pak',
          '<(PRODUCT_DIR)/android_webview/assets/chrome_100_percent.pak',
          '<(PRODUCT_DIR)/android_webview/assets/resources.pak',
          '<(PRODUCT_DIR)/android_webview/assets/en-US.pak',
        ],
        'additional_input_paths': [ '<@(copied_pak_files)' ],
      },
      'actions': [
        {
          'action_name': 'copy_and_strip_so',
          'inputs': ['<(SHARED_LIB_DIR)/libwebview.so'],
          'outputs': ['<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so'],
          'action': [
            '<(android_strip)',
            '--strip-unneeded',  # All symbols not needed for relocation.
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview/assets',
          'files': [ '<@(input_pak_files)' ]
        },
      ],
      'includes': [ '../build/java_apk.gypi' ],
    },
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java',
        '../base/base.gyp:base_java_test_support',
        '../chrome/browser/component/components.gyp:web_contents_delegate_android_java',
        '../chrome/chrome_resources.gyp:packed_extra_resources',
        '../chrome/chrome_resources.gyp:packed_resources',
        '../content/content.gyp:content_java',
        '../content/content.gyp:content_java_test_support',
        '../media/media.gyp:media_java',
        '../net/net.gyp:net_java',
        '../ui/ui.gyp:ui_java',
        'android_webview_java',
        'libwebview',
      ],
      'variables': {
        'package_name': 'android_webview_test',
        'apk_name': 'AndroidWebViewTest',
        'java_in_dir': '../android_webview/javatests',
        'resource_dir': '../res',
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
  ],
}
