# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'open_from_clipboard',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'components_strings.gyp:components_strings',
        'omnibox',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'open_from_clipboard/clipboard_recent_content.h',
        'open_from_clipboard/clipboard_recent_content_ios.h',
        'open_from_clipboard/clipboard_recent_content_ios.mm',
        'open_from_clipboard/clipboard_url_provider.cc',
        'open_from_clipboard/clipboard_url_provider.h',
      ],
    },
  ],
}
