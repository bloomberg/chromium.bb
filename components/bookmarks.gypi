# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'bookmarks_core_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'bookmarks_core_common',
        'components_strings.gyp:components_strings',
        'favicon_base',
        'query_parser',
        'startup_metric_utils',
        'pref_registry',
      ],
      'sources': [
        'bookmarks/core/browser/base_bookmark_model_observer.cc',
        'bookmarks/core/browser/base_bookmark_model_observer.h',
        'bookmarks/core/browser/bookmark_client.cc',
        'bookmarks/core/browser/bookmark_client.h',
        'bookmarks/core/browser/bookmark_codec.cc',
        'bookmarks/core/browser/bookmark_codec.h',
        'bookmarks/core/browser/bookmark_expanded_state_tracker.cc',
        'bookmarks/core/browser/bookmark_expanded_state_tracker.h',
        'bookmarks/core/browser/bookmark_index.cc',
        'bookmarks/core/browser/bookmark_index.h',
        'bookmarks/core/browser/bookmark_match.cc',
        'bookmarks/core/browser/bookmark_match.h',
        'bookmarks/core/browser/bookmark_model.cc',
        'bookmarks/core/browser/bookmark_model.h',
        'bookmarks/core/browser/bookmark_model_observer.h',
        'bookmarks/core/browser/bookmark_node.cc',
        'bookmarks/core/browser/bookmark_node.h',
        'bookmarks/core/browser/bookmark_node_data.cc',
        'bookmarks/core/browser/bookmark_node_data.h',
        'bookmarks/core/browser/bookmark_node_data_ios.cc',
        'bookmarks/core/browser/bookmark_node_data_mac.cc',
        'bookmarks/core/browser/bookmark_node_data_views.cc',
        'bookmarks/core/browser/bookmark_pasteboard_helper_mac.h',
        'bookmarks/core/browser/bookmark_pasteboard_helper_mac.mm',
        'bookmarks/core/browser/bookmark_service.h',
        'bookmarks/core/browser/bookmark_storage.cc',
        'bookmarks/core/browser/bookmark_storage.h',
        'bookmarks/core/browser/bookmark_utils.cc',
        'bookmarks/core/browser/bookmark_utils.h',
        'bookmarks/core/browser/scoped_group_bookmark_actions.cc',
        'bookmarks/core/browser/scoped_group_bookmark_actions.h',
      ],
    },
    {
      'target_name': 'bookmarks_core_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'bookmarks/core/common/bookmark_constants.cc',
        'bookmarks/core/common/bookmark_constants.h',
        'bookmarks/core/common/bookmark_pref_names.cc',
        'bookmarks/core/common/bookmark_pref_names.h',
      ],
    },
    {
      'target_name': 'bookmarks_core_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/platform/events_platform.gyp:events_platform',
        '../url/url.gyp:url_lib',
        'bookmarks_core_browser',
      ],
      'sources': [
        'bookmarks/core/test/bookmark_test_helpers.cc',
        'bookmarks/core/test/bookmark_test_helpers.h',
        'bookmarks/core/test/test_bookmark_client.cc',
        'bookmarks/core/test/test_bookmark_client.h',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../ui/events/platform/x11/x11_events_platform.gyp:x11_events_platform',
          ],
        }],
      ],
    },
  ],
}
