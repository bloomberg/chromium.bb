# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_test_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'dependencies': [
        '../../cast_receiver.gyp:cast_receiver',
        '../../transport/cast_transport.gyp:cast_transport',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',

      ],
      'sources': [
        '<(DEPTH)/media/cast/test/fake_single_thread_task_runner.cc',
        '<(DEPTH)/media/cast/test/fake_single_thread_task_runner.h',
        'audio_utility.cc',
        'audio_utility.h',
        'barcode.cc',
        'barcode.h',
        'default_config.cc',
        'default_config.h',
        'in_process_receiver.cc',
        'in_process_receiver.h',
        'input_builder.cc',
        'input_builder.h',
        'standalone_cast_environment.cc',
        'standalone_cast_environment.h',
        'video_utility.cc',
        'video_utility.h',
      ], # source
    },
    {
      'target_name': 'generate_barcode_video',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/cast/test/utility/utility.gyp:cast_test_utility',
      ],
      'sources': [
        '<(DEPTH)/media/cast/test/utility/generate_barcode_video.cc',
      ],
    },
  ],
}
