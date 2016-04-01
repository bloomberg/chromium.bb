# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //media/gpu/ipc/common
      'target_name': 'media_gpu_ipc_common',
      'type': 'static_library',
      'dependencies': [
        '../../media.gyp:media',
        '../../../base/base.gyp:base',
        '../../../gpu/gpu.gyp:gpu_ipc_common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ui/gfx/gfx.gyp:gfx',
        '../../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
      ],
      # This sources list is duplicated in //media/gpu/ipc/common/BUILD.gn
      'sources': [
        'common/create_video_encoder_params.cc',
        'common/create_video_encoder_params.h',
        'common/media_message_generator.cc',
        'common/media_message_generator.h',
        'common/media_messages.cc',
        'common/media_messages.h',
        'common/media_param_traits.h',
        'common/media_param_traits.cc',
        'common/media_param_traits_macros.h',
      ],
    },
  ]
}
