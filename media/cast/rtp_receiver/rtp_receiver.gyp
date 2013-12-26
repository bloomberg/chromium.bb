# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_rtp_receiver',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
      ],
      'sources': [
        'receiver_stats.cc',
        'receiver_stats.h',
        'rtp_receiver.cc',
        'rtp_receiver.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
        'rtp_parser/rtp_parser.gyp:*',
      ],
    },
  ],
}
