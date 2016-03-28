# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      # GN version: //ios/web/shell:ios_web_shell
      'target_name': 'ios_web_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        'ios_web.gyp:ios_web',
        'ios_web.gyp:ios_web_app',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../net/net.gyp:net_extras',
        '../../ui/base/ui_base.gyp:ui_base',
      ],
      'export_dependent_settings': [
        'ios_web.gyp:ios_web',
        'ios_web.gyp:ios_web_app',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../ui/base/ui_base.gyp:ui_base',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'shell/Info.plist',
        'STRIPFLAGS': '-S',
        # For XCTests a test bundle is injected into the application. Some
        # symbols are required by the tests, and should be included in
        # the file 'ios_web_shell_exported_symbols_list'. The use of
        # -exported_symbols_list ensures that the symbols in this file are
        # exposed. GCC_SYMBOLS_PRIVATE_EXTERN must also be set to 'NO', to
        # ensure symbols aren't hidden and not able to be exposed with
        # -exported_symbols_list. For more information see 'man ld'.
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
        'OTHER_LDFLAGS': [
          '-Xlinker -objc_abi_version',
          '-Xlinker 2',
          '-exported_symbols_list',
          '../../ios/web/ios_web_shell_exported_symbols_list',
        ]
      },
      'sources': [
        'shell/app_delegate.h',
        'shell/app_delegate.mm',
        'shell/shell_browser_state.h',
        'shell/shell_browser_state.mm',
        'shell/shell_main_delegate.h',
        'shell/shell_main_delegate.mm',
        'shell/shell_network_delegate.cc',
        'shell/shell_network_delegate.h',
        'shell/shell_url_request_context_getter.h',
        'shell/shell_url_request_context_getter.mm',
        'shell/shell_web_client.h',
        'shell/shell_web_client.mm',
        'shell/shell_web_main_parts.h',
        'shell/shell_web_main_parts.mm',
        'shell/view_controller.h',
        'shell/view_controller.mm',
        'shell/web_exe_main.mm',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
        'mac_bundle_resources': [
          'shell/Default.png',
          'shell/MainView.xib',
          'shell/textfield_background@2x.png',
          'shell/toolbar_back@2x.png',
          'shell/toolbar_forward@2x.png',
        ],
      },
    },
  ],
}
