# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //gpu:skia_runner
      'target_name': 'skia_runner',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../../gpu/gpu.gyp:command_buffer_service',
        '../../gpu/gpu.gyp:gles2_implementation',
        '../../gpu/gpu.gyp:gl_in_process_context',
        '../../gpu/skia_bindings/skia_bindings.gyp:gpu_skia_bindings',
        '../../skia/skia.gyp:skia',
        '../../third_party/WebKit/public/blink.gyp:blink',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gl/gl.gyp:gl',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'in_process_graphics_system.cc',
        'in_process_graphics_system.h',
        'sk_picture_rasterizer.cc',
        'sk_picture_rasterizer.h',
        'skia_runner.cc',
      ],
    },
  ],
}
