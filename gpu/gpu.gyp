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
      'command_buffer/client/share_group.cc',
      'command_buffer/client/share_group.h',
    ]
  },
  'includes': [
    'gpu_common.gypi',
  ],
  'conditions': [
    ['component=="static_library"', {
      'targets': [
        {
          'target_name': 'gpu',
          'type': 'none',
          'dependencies': [
            'command_buffer_client',
            'command_buffer_common',
            'command_buffer_service',
            'gles2_cmd_helper',
            'gpu_ipc',
          ],
          'sources': [
            'gpu_export.h',
          ],
        },
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
          'target_name': 'gpu_ipc',
          'type': 'static_library',
          'includes': [
            'gpu_ipc.gypi',
          ],
          'dependencies': [
            'command_buffer_common',
          ],
        },
      ],
    },
    { # component != static_library
      'targets': [
        {
          'target_name': 'gpu',
          'type': 'shared_library',
          'includes': [
            'command_buffer_client.gypi',
            'command_buffer_common.gypi',
            'command_buffer_service.gypi',
            'gles2_cmd_helper.gypi',
            'gpu_ipc.gypi',
          ],
          'defines': [
            'GPU_IMPLEMENTATION',
          ],
          'sources': [
            'gpu_export.h',
          ],
        },
        {
          'target_name': 'command_buffer_common',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          # Library helps make GLES2 command buffers.
          'target_name': 'gles2_cmd_helper',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          'target_name': 'command_buffer_client',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          'target_name': 'command_buffer_service',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          'target_name': 'gpu_ipc',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
      ],
    }],
  ],
}
