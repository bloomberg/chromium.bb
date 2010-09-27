# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # Protobuf compiler / generate rule for sync.proto.  This is used by
      # test code in net, which is why it's isolated into its own .gyp file.
      'target_name': 'sync_proto',
      'type': 'none',
      'sources': [
        'sync.proto',
        'encryption.proto',
        'app_specifics.proto',
        'autofill_specifics.proto',
        'bookmark_specifics.proto',
        'extension_specifics.proto',
        'nigori_specifics.proto',
        'password_specifics.proto',
        'preference_specifics.proto',
        'session_specifics.proto',
        'test.proto',
        'theme_specifics.proto',
        'typed_url_specifics.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/pyproto/sync_pb/<(RULE_INPUT_ROOT)_pb2.py',
            '<(SHARED_INTERMEDIATE_DIR)/protoc_out/chrome/browser/sync/protocol/<(RULE_INPUT_ROOT).pb.h',
            '<(SHARED_INTERMEDIATE_DIR)/protoc_out/chrome/browser/sync/protocol/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=.',
            './<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(SHARED_INTERMEDIATE_DIR)/protoc_out/chrome/browser/sync/protocol',
            '--python_out=<(PRODUCT_DIR)/pyproto/sync_pb',
          ],
          'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '../../../../third_party/protobuf2/protobuf.gyp:protoc#host',
      ],
    },
    {
      'target_name': 'sync_proto_cpp',
      'type': 'none',
      'export_dependent_settings': [
        # TODO(akalin): Change back to protobuf_lite once it supports
        # preserving unknown fields.
        '../../../../third_party/protobuf2/protobuf.gyp:protobuf',
        'sync_proto',
      ],
      'dependencies': [
        # TODO(akalin): Change back to protobuf_lite once it supports
        # preserving unknown fields.
        '../../../../third_party/protobuf2/protobuf.gyp:protobuf',
        'sync_proto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
