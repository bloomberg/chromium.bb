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
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'bookmarks/core/browser/bookmark_node.cc',
        'bookmarks/core/browser/bookmark_node.h',
        'bookmarks/core/browser/bookmark_title_match.cc',
        'bookmarks/core/browser/bookmark_title_match.h',
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
  ],
}
