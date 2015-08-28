# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //gpu:command_buffer_gles2
      'target_name': 'command_buffer_gles2',
      'type': 'shared_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gpu/gpu.gyp:gles2_c_lib',
        '../../gpu/gpu.gyp:gles2_implementation',
        '../../gpu/gpu.gyp:command_buffer_service',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../ui/gl/gl.gyp:gl',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        # TODO(hendrikw): Move egl out of gles2_conform_support.
        '../gles2_conform_support/egl/config.cc',
        '../gles2_conform_support/egl/config.h',
        '../gles2_conform_support/egl/display.cc',
        '../gles2_conform_support/egl/display.h',
        '../gles2_conform_support/egl/egl.cc',
        '../gles2_conform_support/egl/surface.cc',
        '../gles2_conform_support/egl/surface.h',
        'command_buffer_egl.cc',
      ],
      'defines': [
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
    },
  ],
}
