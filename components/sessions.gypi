# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/sessions
      'target_name': 'sessions',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../skia/skia.gyp:skia',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../ui/base/ui_base.gyp:ui_base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SESSIONS_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sessions/serialized_navigation_entry.cc',
        'sessions/serialized_navigation_entry.h',
        'sessions/session_id.cc',
        'sessions/session_id.h',
      ],
      'conditions': [
        ['android_webview_build == 0', {
          'dependencies': [
             '../sync/sync.gyp:sync',
          ]
        }],
      ],
    },
    {
      # GN version: //components/sessions:test_support
      'target_name': 'sessions_test_support',
      'type': 'static_library',
      'defines!': ['SESSIONS_IMPLEMENTATION'],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sessions/serialized_navigation_entry_test_helper.cc',
        'sessions/serialized_navigation_entry_test_helper.h',
      ],
      'conditions': [
        ['android_webview_build == 0', {
          'dependencies': [
             '../sync/sync.gyp:sync',
          ]
        }],
      ],
    },
  ],
}
