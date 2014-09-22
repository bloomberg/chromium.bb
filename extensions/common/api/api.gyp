# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions/common/api
      'target_name': 'extensions_api',
      'type': 'static_library',
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../build/json_schema_bundle_compile.gypi',
        '../../../build/json_schema_compile.gypi',
        'schemas.gypi',
      ],
    },
    {
      # Protobuf compiler / generator for chrome.cast.channel-related protocol buffers.
      # GN version: //extensions/browser/api/cast_channel:cast_channel_proto
      'target_name': 'cast_channel_proto',
      'type': 'static_library',
      'sources': [
          'cast_channel/cast_channel.proto',
          'cast_channel/logging.proto'
      ],
      'variables': {
          'proto_in_dir': 'cast_channel',
          'proto_out_dir': 'extensions/common/api/cast_channel',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
