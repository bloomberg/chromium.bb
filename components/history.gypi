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
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'favicon_base',
        'history_core_browser_proto',
        'keyed_service_core',
        'query_parser',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'history/core/browser/history_backend_notifier.h',
        'history/core/browser/history_backend_observer.h',
        'history/core/browser/history_client.cc',
        'history/core/browser/history_client.h',
        'history/core/browser/history_constants.cc',
        'history/core/browser/history_constants.h',
        'history/core/browser/history_db_task.h',
        'history/core/browser/history_match.cc',
        'history/core/browser/history_match.h',
        'history/core/browser/history_service_observer.h',
        'history/core/browser/history_types.cc',
        'history/core/browser/history_types.h',
        'history/core/browser/in_memory_database.cc',
        'history/core/browser/in_memory_database.h',
        'history/core/browser/in_memory_url_index_types.cc',
        'history/core/browser/in_memory_url_index_types.h',
        'history/core/browser/keyword_id.h',
        'history/core/browser/keyword_search_term.cc',
        'history/core/browser/keyword_search_term.h',
        'history/core/browser/page_usage_data.cc',
        'history/core/browser/page_usage_data.h',
        'history/core/browser/thumbnail_database.cc',
        'history/core/browser/thumbnail_database.h',
        'history/core/browser/top_sites_cache.cc',
        'history/core/browser/top_sites_cache.h',
        'history/core/browser/top_sites_observer.h',
        'history/core/browser/url_database.cc',
        'history/core/browser/url_database.h',
        'history/core/browser/url_row.cc',
        'history/core/browser/url_row.h',
        'history/core/browser/url_utils.cc',
        'history/core/browser/url_utils.h',
        'history/core/browser/visit_database.cc',
        'history/core/browser/visit_database.h',
        'history/core/browser/visit_filter.cc',
        'history/core/browser/visit_filter.h',
        'history/core/browser/visit_tracker.cc',
        'history/core/browser/visit_tracker.h',
        'history/core/browser/visitsegment_database.cc',
        'history/core/browser/visitsegment_database.h',
      ],
    },
    {
      # GN version: //components/history/core/browser:proto
      # Protobuf compiler / generator for the InMemoryURLIndex caching
      # protocol buffer.
      'target_name': 'history_core_browser_proto',
      'type': 'static_library',
      'sources': [
        'history/core/browser/in_memory_url_index_cache.proto',
      ],
      'variables': {
        'proto_in_dir': 'history/core/browser',
        'proto_out_dir': 'components/history/core/browser',
      },
      'includes': [ '../build/protoc.gypi' ]
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
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # GN version: //components/history/code/android
          'target_name': 'history_core_android',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../sql/sql.gyp:sql',
            'history_core_browser',
          ],
          'sources': [
            'history/core/android/android_cache_database.cc',
            'history/core/android/android_cache_database.h',
            'history/core/android/android_history_types.cc',
            'history/core/android/android_history_types.h',
            'history/core/android/android_time.h',
            'history/core/android/android_urls_database.cc',
            'history/core/android/android_urls_database.h',
            'history/core/android/favicon_sql_handler.cc',
            'history/core/android/favicon_sql_handler.h',
            'history/core/android/sql_handler.cc',
            'history/core/android/sql_handler.h',
          ],
        },
      ],
    }],
  ],
}
