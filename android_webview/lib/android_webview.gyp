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
        '../../chrome/chrome.gyp:browser',
        '../../chrome/chrome.gyp:renderer',
        '../../content/content.gyp:content',
        '../native/webview_native.gyp:webview_native',
      ],
      'include_dirs': [
        '../..',
        '../../skia/config',
      ],
      'sources': [
        'main/webview_entry_point.cc',
        'main/webview_main_delegate.cc',
        'main/webview_main_delegate.h',
        'main/webview_stubs.cc',
      ],
    },
    {
      'target_name': 'android_webview',
      'type' : 'none',
      'dependencies': [
        'libwebview',
      ],
      'variables': {
        'install_binary_script': '../build/install_binary',
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
          ],
          'action': [
            '<(install_binary_script)',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
