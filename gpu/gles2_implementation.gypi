# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # Library emulates GLES2 using command_buffers.
  'dependencies': [
    '../base/base.gyp:base',
    '../ui/gfx/gl/gl.gyp:gl',
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
}
