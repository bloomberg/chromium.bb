# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_browsing_db
      'target_name': 'safe_browsing_db',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'safe_browsing_db/prefix_set.h',
        'safe_browsing_db/prefix_set.cc',
        'safe_browsing_db/util.h',
        'safe_browsing_db/util.cc',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/safe_browsing_db:proto
      # Protobuf compiler / generator for the Safe Browsing protocol buffer.
      'target_name': 'safe_browsing_proto',
      'type': 'static_library',
      'sources': [ 'safe_browsing_db/safebrowsing.proto' ],
      'variables': {
        'proto_in_dir': 'safe_browsing_db',
        'proto_out_dir': 'components/safe_browsing_db',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
