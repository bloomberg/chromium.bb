# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_framer',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/webrtc',
      ],
      'sources': [
        'cast_message_builder.cc',
        'cast_message_builder.h',
        'frame_buffer.cc',
        'frame_buffer.h',
        'frame_id_map.cc',
        'frame_id_map.h',
        'framer.cc',
        'framer.h',
      ],
    },
  ], # targets
}
