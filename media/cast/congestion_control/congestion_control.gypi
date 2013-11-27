# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'congestion_control',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'congestion_control.h',
        'congestion_control.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
  },
  ],
}

