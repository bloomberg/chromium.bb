# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cma_base',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/base/buffering_controller.cc',
        'cma/base/buffering_controller.h',
        'cma/base/buffering_state.cc',
        'cma/base/buffering_state.h',
        'cma/base/cma_logging.h',
      ],
    },
    {
      'target_name': 'cast_media',
      'type': 'none',
      'dependencies': [
        'cma_base',
      ],
    },
    {
      'target_name': 'cast_media_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_media',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'sources': [
        'cma/base/buffering_controller_unittest.cc',
        'cma/base/run_all_unittests.cc',
      ],
    },
  ],
}
