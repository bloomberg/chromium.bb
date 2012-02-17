# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    'gles2_c_lib',
    '../base/base.gyp:base',
  ],
  'include_dirs': [
    '..',
    '<(DEPTH)/third_party/khronos',
  ],
  'sources': [
    'ipc/gpu_command_buffer_traits.cc',
    'ipc/gpu_command_buffer_traits.h',
  ],
}
