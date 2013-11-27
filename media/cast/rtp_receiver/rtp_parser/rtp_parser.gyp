# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_rtp_parser',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
      ],
      'sources': [
        'rtp_parser.cc',
        'rtp_parser.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
  ],
}
