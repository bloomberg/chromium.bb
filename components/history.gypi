# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/history/core/browser
      'target_name': 'history_core_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'favicon_base',
        'keyed_service_core',
        'query_parser',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'history/core/browser/history_client.cc',
        'history/core/browser/history_client.h',
        'history/core/browser/history_types.cc',
        'history/core/browser/history_types.h',
        'history/core/browser/history_match.cc',
        'history/core/browser/history_match.h',
        'history/core/browser/in_memory_database.cc',
        'history/core/browser/in_memory_database.h',
        'history/core/browser/keyword_id.h',
        'history/core/browser/keyword_search_term.cc',
        'history/core/browser/keyword_search_term.h',
        'history/core/browser/page_usage_data.cc',
        'history/core/browser/page_usage_data.h',
        'history/core/browser/url_database.cc',
        'history/core/browser/url_database.h',
        'history/core/browser/url_row.cc',
        'history/core/browser/url_row.h',
      ],
    },
    {
      # GN version: //components/history/core/common
      'target_name': 'history_core_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'history/core/common/thumbnail_score.cc',
        'history/core/common/thumbnail_score.h',
      ],
    },
    {
      # GN version: //components/history/core/test
      'target_name': 'history_core_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'history_core_browser',
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'history/core/test/history_client_fake_bookmarks.cc',
        'history/core/test/history_client_fake_bookmarks.h',
      ],
    },
  ],
}
