# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../ipc/ipc.gyp:ipc',
  ],
  'include_dirs': [
    '..',
    '<(DEPTH)/third_party/khronos',
  ],
  'sources': [
    'ipc/command_buffer_proxy.h',
    'ipc/command_buffer_proxy.cc',
    'ipc/gpu_command_buffer_traits.cc',
    'ipc/gpu_command_buffer_traits.h',
  ],
}
