# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    # The public GCM target.
    {
      'target_name': 'gcm',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'proto_in_dir': './protocol',
        'proto_out_dir': 'google_apis/gcm/protocol',
        'cc_generator_options': 'dllexport_decl=GCM_EXPORT:',
        'cc_include': 'google_apis/gcm/base/gcm_export.h',
      },
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'GCM_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../net/net.gyp:net',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'sources': [
        'base/mcs_util.h',
        'base/mcs_util.cc',
        'base/socket_stream.h',
        'base/socket_stream.cc',
        'engine/connection_handler.h',
        'engine/connection_handler.cc',
        'gcm_client.cc',
        'gcm_client.h',
        'gcm_client_impl.cc',
        'gcm_client_impl.h',
        'protocol/mcs.proto',
      ],
      'includes': [
        '../../build/protoc.gypi'
      ],
    },

    # The main GCM unit tests.
    {
      'target_name': 'gcm_unit_tests',
      'type': '<(gtest_target_type)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'gcm'
      ],
      'sources': [
        'base/mcs_util_unittest.cc',
        'base/socket_stream_unittest.cc',
        'engine/connection_handler_unittest.cc',
      ]
    },
  ],
}
