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
      # Protobuf compiler / generate rule for the device management protocol.
      'target_name': 'device_management_proto',
      'type': 'none',
      'sources': [
        'cloud_policy.proto',
        'device_management_backend.proto',
        'device_management_local.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'variables': {
            # The protoc compiler requires a proto_path argument with the
            # directory containing the .proto file. There's no generator
            # variable that corresponds to this, so fake it.
            'rule_input_relpath': 'chrome/browser/policy/proto',
          },
          'outputs': [
            '<(PRODUCT_DIR)/pyproto/device_management_pb/<(RULE_INPUT_ROOT)_pb2.py',
            '<(protoc_out_dir)/<(rule_input_relpath)/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/<(rule_input_relpath)/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=.',
            './<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/<(rule_input_relpath)',
            '--python_out=<(PRODUCT_DIR)/pyproto/device_management_pb',
          ],
          'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '../../../../third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
    },
    {
      'target_name': 'device_management_proto_cpp',
      'type': 'none',
      'export_dependent_settings': [
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'device_management_proto',
      ],
      'dependencies': [
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'device_management_proto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
