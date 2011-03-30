# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
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
            '<(protoc_out_dir)/chrome/browser/sync/protocol/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/chrome/browser/sync/protocol/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=.',
            './<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/chrome/browser/sync/protocol',
            '--python_out=<(PRODUCT_DIR)/pyproto/sync_pb',
          ],
          'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '../../../../third_party/protobuf/protobuf.gyp:protoc#host',
      ],
    },
    {
      'target_name': 'sync_proto_cpp',
      'type': '<(library)',
      'sources': [
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/encryption.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/encryption.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/app_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/app_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/autofill_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/autofill_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/bookmark_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/bookmark_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/extension_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/extension_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/nigori_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/nigori_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/password_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/password_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/preference_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/preference_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/session_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/session_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/theme_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/theme_specifics.pb.h',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/typed_url_specifics.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/typed_url_specifics.pb.h',
      ],
      'export_dependent_settings': [
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'sync_proto',
      ],
      'dependencies': [
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'sync_proto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ],
      },
      # This target exports a hard dependency because it includes generated
      # header files.
      'hard_dependency': 1,
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
