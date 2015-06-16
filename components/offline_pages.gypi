# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/offline_pages:offline_pages
      'target_name': 'offline_pages',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'keyed_service_core',
      ],
      'sources': [
        'offline_pages/offline_page_archiver.h',
        'offline_pages/offline_page_item.cc',
        'offline_pages/offline_page_item.h',
        'offline_pages/offline_page_model.cc',
        'offline_pages/offline_page_model.h',
        'offline_pages/offline_page_metadata_store.cc',
        'offline_pages/offline_page_metadata_store.h',
      ],
    },
  ],
}
