# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      # GN version: //ios/public/test
      'target_name': 'test_support_ios',
      'type': 'static_library',
      'sources': [
        # TODO(crbug.com/437333): Move most of these files to
        # //ios/chrome/ios_chrome_tests.gyp.
        'public/test/fake_sync_service_factory.cc',
        'public/test/fake_sync_service_factory.h',
        'public/test/test_chrome_browser_provider.h',
        'public/test/test_chrome_browser_provider.mm',
        'public/test/test_chrome_provider_initializer.cc',
        'public/test/test_chrome_provider_initializer.h',
        'public/test/test_updatable_resource_provider.h',
        'public/test/test_updatable_resource_provider.mm',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:keyed_service_core',
        '../components/components.gyp:keyed_service_ios',
        '../components/components.gyp:sync_driver_test_support',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
        'provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
