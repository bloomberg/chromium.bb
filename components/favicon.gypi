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
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
        'favicon_base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'favicon/core/favicon_driver.h',
        'favicon/core/favicon_url.cc',
        'favicon/core/favicon_url.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # GN version: //components/favicon/core/browser
      'target_name': 'favicon_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'bookmarks_browser',
        'favicon_base',
        'favicon_core',
        'history_core_browser',
        'keyed_service_core',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'favicon/core/browser/favicon_client.h',
        'favicon/core/browser/favicon_handler.cc',
        'favicon/core/browser/favicon_handler.h',
        'favicon/core/browser/favicon_service.cc',
        'favicon/core/browser/favicon_service.h',
        'favicon/core/browser/favicon_tab_helper_observer.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
