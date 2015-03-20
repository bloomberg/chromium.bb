# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/favicon/core
      'target_name': 'favicon_core',
      'type': 'static_library',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'bookmarks_browser',
        'favicon_base',
        'history_core_browser',
        'keyed_service_core',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'favicon/core/favicon_client.h',
        'favicon/core/favicon_driver.h',
        'favicon/core/favicon_handler.cc',
        'favicon/core/favicon_handler.h',
        'favicon/core/favicon_service.cc',
        'favicon/core/favicon_service.h',
        'favicon/core/favicon_tab_helper_observer.h',
        'favicon/core/favicon_url.cc',
        'favicon/core/favicon_url.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
