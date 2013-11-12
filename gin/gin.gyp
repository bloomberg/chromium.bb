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
        'arguments.cc',
        'arguments.h',
        'array_buffer.cc',
        'array_buffer.h',
        'converter.cc',
        'converter.h',
        'dictionary.cc',
        'dictionary.h',
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
      'target_name': 'gin_test',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'gin',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'gin',
      ],
      'sources': [
        'test/gtest.cc',
        'test/gtest.h',
        'test/v8_test.cc',
        'test/v8_test.h',
      ],
    },
    {
      'target_name': 'gin_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        'gin_test',
      ],
      'sources': [
        'converter_unittest.cc',
        'test/run_all_unittests.cc',
        'runner_unittest.cc',
      ],
    },
  ],
}
