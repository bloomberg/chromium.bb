# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios',
      'type': 'none',
      'dependencies': [
        'chrome/ios_chrome_tests.gyp:*',
        'provider/ios_provider_chrome.gyp:*',
        'provider/ios_provider_web.gyp:*',
        'net/ios_net.gyp:*',
        'net/ios_net_unittests.gyp:*',
        'web/ios_web.gyp:*',
        'web/ios_web_unittests.gyp:*',
      ],
    },
  ],
}
