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
        '../../components/components.gyp:infobars_core',
        '../../components/components.gyp:keyed_service_core',
        '../../components/components.gyp:keyed_service_ios',
        '../../components/components.gyp:leveldb_proto',
        '../../components/components.gyp:suggestions',
        '../../components/components.gyp:translate_core_browser',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
        '../web/ios_web.gyp:ios_web',
      ],
      'sources': [
        'browser/browser_state/browser_state_otr_helper.cc',
        'browser/browser_state/browser_state_otr_helper.h',
        'browser/infobars/confirm_infobar_controller.h',
        'browser/infobars/confirm_infobar_controller.mm',
        'browser/infobars/confirm_infobar_delegate.mm',
        'browser/infobars/infobar.h',
        'browser/infobars/infobar.mm',
        'browser/infobars/infobar_container_ios.h',
        'browser/infobars/infobar_container_ios.mm',
        'browser/infobars/infobar_container_view.h',
        'browser/infobars/infobar_container_view.mm',
        'browser/infobars/infobar_controller.h',
        'browser/infobars/infobar_controller.mm',
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
