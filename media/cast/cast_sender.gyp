# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
#    'audio_sender/audio_sender.gypi',
#    'video_sender/video_sender.gypi',
  ],
  'targets': [
    {
      'target_name': 'cast_sender_impl',
      'type': 'static_library',
      'sources': [
        'cast_sender.h',
#        'cast_sender_impl.cc',
#        'cast_sender_impl.h',
      ], # source
      'dependencies': [
#        '<(DEPTH)/media/cast/pacing/paced_sender.gyp:*',
#        'audio_sender',
#        'video_sender',
      ], # dependencies
    },
  ],
}
