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
      'command_buffer/client/gles2_implementation_autogen.h',
      'command_buffer/client/gles2_implementation.cc',
      'command_buffer/client/gles2_implementation.h',
      'command_buffer/client/program_info_manager.cc',
      'command_buffer/client/program_info_manager.h',
    ]
  },
  'targets': [
    {
      'target_name': 'command_buffer_common',
      'type': 'static_library',
      'includes': [
        'command_buffer_common.gypi',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
    },
    {
      # Library helps make GLES2 command buffers.
      'target_name': 'gles2_cmd_helper',
      'type': 'static_library',
      'includes': [
        'gles2_cmd_helper.gypi',
      ],
      'dependencies': [
        'command_buffer_client',
      ],
    },
    {
      # Library emulates GLES2 using command_buffers.
      'target_name': 'gles2_implementation',
      'type': 'static_library',
      'includes': [
        'gles2_implementation.gypi',
      ],
      'dependencies': [
        'gles2_cmd_helper',
      ],
    },
    {
      # Library emulates GLES2 using command_buffers.
      'target_name': 'gles2_implementation_client_side_arrays',
      'type': 'static_library',
      'defines': [
        'GLES2_SUPPORT_CLIENT_SIDE_ARRAYS=1',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gles2_cmd_helper',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          # For GLES2/gl2.h
          '<(DEPTH)/third_party/khronos',
        ],
      },
      'sources': [
        '<@(gles2_implementation_source_files)',
      ],
    },
    {
      # Library emulates GLES2 using command_buffers.
      'target_name': 'gles2_implementation_client_side_arrays_no_check',
      'type': 'static_library',
      'defines': [
        'GLES2_SUPPORT_CLIENT_SIDE_ARRAYS=1',
        'GLES2_CONFORMANCE_TESTS=1',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gles2_cmd_helper',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          # For GLES2/gl2.h
          '<(DEPTH)/third_party/khronos',
        ],
      },
      'sources': [
        '<@(gles2_implementation_source_files)',
      ],
    },
    {
      # Stub to expose gles2_implemenation in C instead of C++.
      # so GLES2 C programs can work with no changes.
      'target_name': 'gles2_c_lib',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'gles2_implementation',
      ],
      'defines': [
        'GLES2_C_LIB_IMPLEMENTATION',
      ],
      'sources': [
        '<@(gles2_c_lib_source_files)',
      ],
    },
    {
      # Same as gles2_c_lib except with no parameter checking. Required for
      # OpenGL ES 2.0 conformance tests.
      'target_name': 'gles2_c_lib_nocheck',
      'type': '<(component)',
      'defines': [
        'GLES2_C_LIB_IMPLEMENTATION',
        'GLES2_CONFORMANCE_TESTS=1',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'gles2_implementation_client_side_arrays_no_check',
      ],
      'sources': [
        '<@(gles2_c_lib_source_files)',
      ],
    },
    {
      'target_name': 'command_buffer_client',
      'type': 'static_library',
      'includes': [
        'command_buffer_client.gypi',
      ],
      'dependencies': [
        'command_buffer_common',
      ],
    },
    {
      'target_name': 'command_buffer_service',
      'type': 'static_library',
      'includes': [
        'command_buffer_service.gypi',
      ],
      'dependencies': [
        'command_buffer_common',
      ],
    },
    {
      'target_name': 'gpu_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gmock.gyp:gmock',
        '../testing/gmock.gyp:gmock_main',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gl/gl.gyp:gl',
        'command_buffer_client',
        'command_buffer_common',
        'command_buffer_service',
        'gpu_unittest_utils',
        'gles2_implementation_client_side_arrays',
        'gles2_cmd_helper',
      ],
      'defines': [
        'GLES2_C_LIB_IMPLEMENTATION',
      ],
      'sources': [
        '<@(gles2_c_lib_source_files)',
        'command_buffer/client/client_test_helper.cc',
        'command_buffer/client/client_test_helper.h',
        'command_buffer/client/cmd_buffer_helper_test.cc',
        'command_buffer/client/fenced_allocator_test.cc',
        'command_buffer/client/gles2_implementation_unittest.cc',
        'command_buffer/client/mapped_memory_unittest.cc',
        'command_buffer/client/program_info_manager_unittest.cc',
        'command_buffer/client/ring_buffer_test.cc',
        'command_buffer/client/transfer_buffer_unittest.cc',
        'command_buffer/common/bitfield_helpers_test.cc',
        'command_buffer/common/command_buffer_mock.cc',
        'command_buffer/common/command_buffer_mock.h',
        'command_buffer/common/command_buffer_shared_test.cc',
        'command_buffer/common/gles2_cmd_format_test.cc',
        'command_buffer/common/gles2_cmd_format_test_autogen.h',
        'command_buffer/common/gles2_cmd_utils_unittest.cc',
        'command_buffer/common/id_allocator_test.cc',
        'command_buffer/common/trace_event.h',
        'command_buffer/common/unittest_main.cc',
        'command_buffer/service/buffer_manager_unittest.cc',
        'command_buffer/service/cmd_parser_test.cc',
        'command_buffer/service/common_decoder_unittest.cc',
        'command_buffer/service/context_group_unittest.cc',
        'command_buffer/service/feature_info_unittest.cc',
        'command_buffer/service/framebuffer_manager_unittest.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_1.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_1_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_2.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_3.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_3_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_base.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_base.h',
        'command_buffer/service/gpu_scheduler_unittest.cc',
        'command_buffer/service/id_manager_unittest.cc',
        'command_buffer/service/mocks.cc',
        'command_buffer/service/mocks.h',
        'command_buffer/service/program_manager_unittest.cc',
        'command_buffer/service/renderbuffer_manager_unittest.cc',
        'command_buffer/service/shader_manager_unittest.cc',
        'command_buffer/service/shader_translator_unittest.cc',
        'command_buffer/service/stream_texture_mock.cc',
        'command_buffer/service/stream_texture_mock.h',
        'command_buffer/service/stream_texture_manager_mock.cc',
        'command_buffer/service/stream_texture_manager_mock.h',
        'command_buffer/service/test_helper.cc',
        'command_buffer/service/test_helper.h',
        'command_buffer/service/texture_manager_unittest.cc',
        'command_buffer/service/vertex_attrib_manager_unittest.cc',
      ],
    },
    {
      'target_name': 'gpu_unittest_utils',
      'type': 'static_library',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gl/gl.gyp:gl',
      ],
      'include_dirs': [
        '..',
        '<(DEPTH)/third_party/khronos',
      ],
      'sources': [
        'command_buffer/common/gl_mock.h',
        'command_buffer/common/gl_mock.cc',
        'command_buffer/service/gles2_cmd_decoder_mock.cc',
        'command_buffer/service/gles2_cmd_decoder_mock.cc',
      ],
    },
    {
      'target_name': 'gpu_ipc',
      'type': 'static_library',
      'includes': [
        'gpu_ipc.gypi',
      ],
      'dependencies': [
        'command_buffer_client',
      ],
    },
  ],
}
