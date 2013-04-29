# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sessions',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../content/content.gyp:content_browser',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SESSIONS_IMPLEMENTATION',
      ],
      'sources': [
        'sessions/serialized_navigation_entry.cc',
        'sessions/serialized_navigation_entry.h',
      ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            '../webkit/support/webkit_support.gyp:glue',
          ]
        }],
      ],
    },
    {
      'target_name': 'sessions_test_support',
      'type': 'static_library',
      'defines!': ['SESSIONS_IMPLEMENTATION'],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'sessions/serialized_navigation_entry_test_helper.cc',
        'sessions/serialized_navigation_entry_test_helper.h',
      ],
    },
  ],
}
