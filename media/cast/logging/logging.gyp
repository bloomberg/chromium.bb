# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'targets': [
    {
      'target_name': 'cast_logging',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'logging_defines.cc',
        'logging_defines.h',
        'logging_impl.cc',
        'logging_impl.h',
        'logging_raw.cc',
        'logging_raw.h',
        'logging_stats.cc',
        'logging_stats.h',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
      ],
    },
  ], # targets
}
