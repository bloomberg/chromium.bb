# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'user_prefs',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'user_prefs/user_prefs.cc',
        'user_prefs/user_prefs.h',
      ],
    },
  ],
}
