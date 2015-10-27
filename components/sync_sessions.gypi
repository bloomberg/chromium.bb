# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/sync_sessions
      'target_name': 'sync_sessions',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        'bookmarks_browser',
        'history_core_browser',
        'sync_driver',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sync_sessions/favicon_cache.cc',
        'sync_sessions/favicon_cache.h',
        'sync_sessions/revisit/bookmarks_by_url_provider_impl.cc',
        'sync_sessions/revisit/bookmarks_by_url_provider_impl.h',
        'sync_sessions/revisit/bookmarks_page_revisit_observer.cc',
        'sync_sessions/revisit/bookmarks_page_revisit_observer.h',
        'sync_sessions/synced_tab_delegate.cc',
        'sync_sessions/synced_tab_delegate.h',
        'sync_sessions/sync_sessions_client.cc',
        'sync_sessions/sync_sessions_client.h',
        'sync_sessions/revisit/current_tab_matcher.cc',
        'sync_sessions/revisit/current_tab_matcher.h',
        'sync_sessions/revisit/offset_tab_matcher.cc',
        'sync_sessions/revisit/offset_tab_matcher.h',
        'sync_sessions/revisit/page_equality.h',
        'sync_sessions/revisit/page_visit_observer.h',
        'sync_sessions/revisit/sessions_page_revisit_observer.cc',
        'sync_sessions/revisit/sessions_page_revisit_observer.h',
        'sync_sessions/revisit/typed_url_page_revisit_observer.cc',
        'sync_sessions/revisit/typed_url_page_revisit_observer.h',
        'sync_sessions/revisit/typed_url_page_revisit_task.cc',
        'sync_sessions/revisit/typed_url_page_revisit_task.h',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            'sessions_content',
          ],
        }, {  # OS==ios
          'dependencies': [
            'sessions_ios',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_sessions_test_support',
      'type': 'static_library',
      'dependencies': [
        'sync_sessions',
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'sync_sessions/fake_sync_sessions_client.cc',
        'sync_sessions/fake_sync_sessions_client.h',
      ],
    }
  ]
}
