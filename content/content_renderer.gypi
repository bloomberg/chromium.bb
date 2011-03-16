# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_renderer',
      'msvs_guid': '9AAA8CF2-9B3D-4895-8CB9-D70BBD125EAD',
      'type': '<(library)',
      'dependencies': [
        'content_common',
        '../skia/skia.gyp:skia',
        '../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'renderer/ggl.cc',
        'renderer/ggl.h',
        'renderer/media/audio_renderer_impl.cc',
        'renderer/media/audio_renderer_impl.h',
        'renderer/media/gles2_video_decode_context.cc',
        'renderer/media/gles2_video_decode_context.h',
        'renderer/media/ipc_video_decoder.cc',
        'renderer/media/ipc_video_decoder.h',
        'renderer/p2p/ipc_network_manager.cc',
        'renderer/p2p/ipc_network_manager.h',
        'renderer/p2p/ipc_socket_factory.cc',
        'renderer/p2p/ipc_socket_factory.h',
        'renderer/p2p/socket_client.cc',
        'renderer/p2p/socket_client.h',
        'renderer/p2p/socket_dispatcher.cc',
        'renderer/p2p/socket_dispatcher.h',
      ],
    },
  ],
}
