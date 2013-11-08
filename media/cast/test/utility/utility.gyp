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
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'input_helper.cc',
        'input_helper.h',
        '<(DEPTH)/media/cast/test/audio_utility.cc',
        '<(DEPTH)/media/cast/test/fake_task_runner.cc',
        '<(DEPTH)/media/cast/test/video_utility.cc',
      ], # source
    },
  ],
}