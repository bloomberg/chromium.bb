# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copresence',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
        '../net/net.gyp:net',
        'copresence_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'copresence/copresence_client.cc',
        'copresence/copresence_constants.cc',
        'copresence/handlers/audio/audio_directive_handler.cc',
        'copresence/handlers/audio/audio_directive_handler.h',
        'copresence/handlers/audio/audio_directive_list.cc',
        'copresence/handlers/audio/audio_directive_list.h',
        'copresence/handlers/directive_handler.cc',
        'copresence/handlers/directive_handler.h',
        'copresence/mediums/audio/audio_player.cc',
        'copresence/mediums/audio/audio_player.h',
        'copresence/mediums/audio/audio_recorder.cc',
        'copresence/mediums/audio/audio_recorder.h',
        'copresence/public/copresence_client_delegate.h',
        'copresence/public/copresence_client.h',
        'copresence/public/copresence_constants.h',
        'copresence/public/whispernet_client.h',
        'copresence/rpc/rpc_handler.cc',
        'copresence/rpc/rpc_handler.h'
        'copresence/timed_map.h',
      ],
      'export_dependent_settings': [
        'copresence_proto',
      ],
    },
    {
      'target_name': 'copresence_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'copresence/test/audio_test_support.cc',
        'copresence/test/audio_test_support.h',
      ],
    },
    {
      # Protobuf compiler / generate rule for copresence.
      # GN version: //components/copresence/proto
      # Note: These protos are auto-generated from the protos of the
      # Copresence server. Currently this strips all formatting and comments
      # but makes sure that we are always using up to date protos.
      'target_name': 'copresence_proto',
      'type': 'static_library',
      'sources': [
        'copresence/proto/codes.proto',
        'copresence/proto/data.proto',
        'copresence/proto/enums.proto',
        'copresence/proto/identity.proto',
        'copresence/proto/rpcs.proto',
      ],
      'variables': {
        'proto_in_dir': 'copresence/proto',
        'proto_out_dir': 'components/copresence/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
