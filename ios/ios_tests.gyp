# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'test_support_ios',
      'type': 'static_library',
      'sources': [
        'public/test/fake_profile_oauth2_token_service_ios_provider.h',
        'public/test/fake_profile_oauth2_token_service_ios_provider.mm',
        'public/test/fake_string_provider.cc',
        'public/test/fake_string_provider.h',
        'public/test/test_chrome_browser_provider.h',
        'public/test/test_chrome_browser_provider.mm',
        'public/test/test_chrome_provider_initializer.cc',
        'public/test/test_chrome_provider_initializer.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
