# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //gpu/blink
      'target_name': 'gpu_blink',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../../gpu/gpu.gyp:command_buffer_common',
        '../../gpu/gpu.gyp:gles2_c_lib',
        '../../gpu/gpu.gyp:gles2_implementation',
        '../../gpu/skia_bindings/skia_bindings.gyp:gpu_skia_bindings',
        '../../third_party/WebKit/public/blink.gyp:blink_minimal',
      ],
      'defines': [
        'GPU_BLINK_IMPLEMENTATION',
      ],
      # This sources list is duplicated in //gpu/blink/BUILD.gn
      'sources': [
        'gpu_blink_export.h',
        'webgraphicscontext3d_impl.cc',
        'webgraphicscontext3d_impl.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ]
}
