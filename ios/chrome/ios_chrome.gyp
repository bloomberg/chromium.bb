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
        '../../components/components.gyp:keyed_service_core',
        '../../components/components.gyp:keyed_service_ios',
        '../../components/components.gyp:leveldb_proto',
        '../../components/components.gyp:suggestions',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
        '../web/ios_web.gyp:ios_web',
      ],
      'sources': [
        'browser/net/image_fetcher.h',
        'browser/net/image_fetcher.mm',
        'browser/suggestions/image_fetcher_impl.h',
        'browser/suggestions/image_fetcher_impl.mm',
        'browser/suggestions/suggestions_service_factory.h',
        'browser/suggestions/suggestions_service_factory.mm',
      ],
    },
  ],
}
