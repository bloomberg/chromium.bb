# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'input_method_out_dir':
      '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/chromeos/input_method',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
  },
  'targets': [
    {
      'target_name': 'gencode',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'ibus_input_methods.txt',
            'gen_ibus_input_methods.py',
          ],
          'outputs': [
            '<(input_method_out_dir)/ibus_input_methods.h',
          ],
          'action_name': 'gen_ibus_input_methods',
          'action': [
            'python',
            'gen_ibus_input_methods.py',
            'ibus_input_methods.txt',
            '<(input_method_out_dir)/ibus_input_methods.h',
          ],
          'message': 'Generating ibus_input_methods.h',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
    {
      'target_name': 'litify_mozc_proto_files',
      'type': 'none',
      'actions': [
        {
          'action_name': 'litify_config_proto',
          'inputs': [
            '../../../../third_party/mozc/session/config.proto',
          ],
          'outputs': [
            '<(input_method_out_dir)/mozc/session/config.proto',
          ],
          'action': [
            'python',
            'litify_proto_file.py',
            '../../../../third_party/mozc/session/config.proto',
            '<(input_method_out_dir)/mozc/session/config.proto',
          ],
        },
        {
          'action_name': 'litify_commands_proto',
          'inputs': [
            '../../../../third_party/mozc/session/commands.proto',
          ],
          'outputs': [
            '<(input_method_out_dir)/mozc/session/commands.proto',
          ],
          'action': [
            'python',
            'litify_proto_file.py',
            '../../../../third_party/mozc/session/commands.proto',
            '<(input_method_out_dir)/mozc/session/commands.proto',
          ],
        },
      ],
    },
    {
      # Protobuf compiler / generator for the mozc inputmethod commands
      # protocol buffer.
      'target_name': 'mozc_commands_proto',
      'type': 'static_library',
      'sources': [
        '<(input_method_out_dir)/mozc/session/config.proto',
        '<(input_method_out_dir)/mozc/session/commands.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/third_party/mozc/session/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/third_party/mozc/session/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(input_method_out_dir)/mozc',
            '<(input_method_out_dir)/mozc/session/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/third_party/mozc',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
          'process_outputs_as_sources': 1,
        },
      ],
      'dependencies': [
        'litify_mozc_proto_files',
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../../../../third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'include_dirs': [
        '<(protoc_out_dir)/third_party/mozc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)/third_party/mozc',
        ]
      },
      'export_dependent_settings': [
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
  ]
}
