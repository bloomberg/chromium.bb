# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_paced_sender',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'paced_sender.h',
        'paced_sender.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
  ], # targets
}
