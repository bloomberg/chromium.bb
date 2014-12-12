# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_chrome_browser',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../url/url.gyp:url_lib',
        '../web/ios_web.gyp:ios_web',
      ],
      'sources': [
        'browser/net/image_fetcher.h',
        'browser/net/image_fetcher.mm',
      ],
    },
  ],
}
