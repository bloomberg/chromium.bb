# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # These are defined here because we need to build this library twice. Once
    # with extra parameter checking. Once with no parameter checking to be 100%
    # OpenGL ES 2.0 compliant for the conformance tests.
    'gles2_c_lib_source_files': [
      'command_buffer/client/gles2_c_lib.cc',
      'command_buffer/client/gles2_c_lib_autogen.h',
      'command_buffer/client/gles2_c_lib_export.h',
      'command_buffer/client/gles2_lib.h',
      'command_buffer/client/gles2_lib.cc',
    ],
    # These are defined here because we need to build this library twice. Once
    # with without support for client side arrays and once with for pepper and
    # the OpenGL ES 2.0 compliant for the conformance tests.
    'gles2_implementation_source_files': [
      'command_buffer/client/gles2_impl_export.h',
      'command_buffer/client/gles2_implementation_autogen.h',
      'command_buffer/client/gles2_implementation.cc',
      'command_buffer/client/gles2_implementation.h',
      'command_buffer/client/program_info_manager.cc',
      'command_buffer/client/program_info_manager.h',
      'command_buffer/client/query_tracker.cc',
      'command_buffer/client/query_tracker.h',
      'command_buffer/client/share_group.cc',
      'command_buffer/client/share_group.h',
    ]
  },
  'conditions': [
    # Special target to wrap a gtest_target_type==shared_library
    # gpu_unittests into an android apk for execution.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'gpu_unittests_apk',
          'type': 'none',
          'dependencies': [
            'gpu_unittests',
          ],
          'variables': {
            'test_suite_name': 'gpu_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)gpu_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
