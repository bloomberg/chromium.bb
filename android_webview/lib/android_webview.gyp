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
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'main/webview_entry_point.cc',
        'main/webview_main_delegate.cc',
        'main/webview_main_delegate.h',
        'main/webview_stubs.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
