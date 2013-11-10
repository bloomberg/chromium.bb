# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gin',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'dependencies': [
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'sources': [
        'array_buffer.cc',
        'array_buffer.h',
        'converter.cc',
        'converter.h',
        'initialize.cc',
        'initialize.h',
        'per_isolate_data.cc',
        'per_isolate_data.h',
        'runner.cc',
        'runner.h',
        'wrapper_info.cc',
        'wrapper_info.h',
      ],
    },
    {
      'target_name': 'gin_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        'gin',
      ],
      'sources': [
        'test/v8_test.cc',
        'test/run_all_unittests.cc',
        'converter_unittest.cc',
        'runner_unittest.cc',
      ],
    },
  ],
}
