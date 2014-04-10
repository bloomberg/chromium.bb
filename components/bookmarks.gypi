# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
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
        'bookmarks/core/common/bookmark_pref_names.cc',
        'bookmarks/core/common/bookmark_pref_names.h',
      ],
    },
  ],
}
