# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '<(DEPTH)/third_party/khronos',
  ],
  'all_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)/third_party/khronos',
    ],
  },
  'dependencies': [
    '../base/base.gyp:base',
    'command_buffer/command_buffer.gyp:gles2_utils',
  ],
  'sources': [
    'command_buffer/common/bitfield_helpers.h',
    'command_buffer/common/buffer.h',
    'command_buffer/common/cmd_buffer_common.h',
    'command_buffer/common/cmd_buffer_common.cc',
    'command_buffer/common/command_buffer.h',
    'command_buffer/common/compiler_specific.h',
    'command_buffer/common/constants.h',
    'command_buffer/common/gles2_cmd_ids_autogen.h',
    'command_buffer/common/gles2_cmd_ids.h',
    'command_buffer/common/gles2_cmd_format_autogen.h',
    'command_buffer/common/gles2_cmd_format.cc',
    'command_buffer/common/gles2_cmd_format.h',
    'command_buffer/common/id_allocator.cc',
    'command_buffer/common/id_allocator.h',
    'command_buffer/common/thread_local.h',
    'command_buffer/common/types.h',
  ],
}
