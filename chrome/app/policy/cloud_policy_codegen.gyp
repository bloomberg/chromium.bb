# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'policy_out_dir': '<(SHARED_INTERMEDIATE_DIR)/policy',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'generate_policy_source_script_path':
        '<(DEPTH)/chrome/tools/build/generate_policy_source.py',
    'policy_constant_header_path':
        '<(policy_out_dir)/policy/policy_constants.h',
    'policy_constant_source_path':
        '<(policy_out_dir)/policy/policy_constants.cc',
    'configuration_policy_type_header_path':
        '<(policy_out_dir)/policy/configuration_policy_type.h',
    'protobuf_decoder_path':
        '<(policy_out_dir)/policy/cloud_policy_generated.cc',
    'cloud_policy_proto_path': '<(policy_out_dir)/policy/cloud_policy.proto',
    'proto_path_substr': 'chrome/browser/policy/proto',
    'proto_rel_path': '<(DEPTH)/<(proto_path_substr)',
  },
  'targets': [
    {
      'target_name': 'cloud_policy_code_generate',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'policy_templates.json',
            '<(generate_policy_source_script_path)',
          ],
          'outputs': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
            '<(configuration_policy_type_header_path)',
            '<(protobuf_decoder_path)',
            '<(cloud_policy_proto_path)',
          ],
          'action_name': 'generate_policy_source',
          'action': [
            'python',
            '<@(generate_policy_source_script_path)',
            '--policy-constants-header=<(policy_constant_header_path)',
            '--policy-constants-source=<(policy_constant_source_path)',
            '--policy-type-header=<(configuration_policy_type_header_path)',
            '--policy-protobuf=<(cloud_policy_proto_path)',
            '--protobuf-decoder=<(protobuf_decoder_path)',
            '<(OS)',
            'policy_templates.json',
          ],
          'message': 'Generating policy source',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(policy_out_dir)',
          '<(protoc_out_dir)',
        ],
      },
    },
    {
      'target_name': 'cloud_policy_proto_compile',
      'type': 'none',
      'actions': [
        {
          'action_name': 'compile_generated_proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '<(policy_out_dir)/policy/cloud_policy.proto',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/pyproto/device_management_pb/cloud_policy_pb2.py',
            '<(protoc_out_dir)/<(proto_path_substr)/cloud_policy.pb.h',
            '<(protoc_out_dir)/<(proto_path_substr)/cloud_policy.pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(policy_out_dir)/policy',
            '<(policy_out_dir)/policy/cloud_policy.proto',
            '--cpp_out=<(protoc_out_dir)/<(proto_path_substr)',
            '--python_out=<(PRODUCT_DIR)/pyproto/device_management_pb',
          ],
          'message': 'Compiling generated cloud policy protobuf',
        },
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
        'cloud_policy_code_generate',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
    },
    {
      'target_name': 'cloud_policy_backend_header_compile',
      'type': 'none',
      'sources': [
        '<(proto_rel_path)/device_management_backend.proto',
        '<(proto_rel_path)/device_management_local.proto',
      ],
      'rules': [
        {
          'rule_name': 'gen_proto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/pyproto/device_management_pb/<(RULE_INPUT_ROOT)_pb2.py',
            '<(protoc_out_dir)/<(proto_path_substr)/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/<(proto_path_substr)/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(policy_out_dir)/policy',
            '--proto_path=<(proto_rel_path)',
            '<(proto_rel_path)/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)/<(proto_path_substr)',
            '--python_out=<(PRODUCT_DIR)/pyproto/device_management_pb',
          ],
          'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
        }
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
        'cloud_policy_proto_compile',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
    },
    {
      'target_name': 'policy',
      'type': '<(library)',
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(policy_out_dir)',
          '<(protoc_out_dir)',
        ],
      },
      'sources': [
        '<(policy_constant_header_path)',
        '<(policy_constant_source_path)',
        '<(configuration_policy_type_header_path)',
        '<(protobuf_decoder_path)',
        '<(protoc_out_dir)/<(proto_path_substr)/cloud_policy.pb.h',
        '<(protoc_out_dir)/<(proto_path_substr)/cloud_policy.pb.cc',
        '<(DEPTH)/chrome/browser/policy/policy_map.h',
        '<(DEPTH)/chrome/browser/policy/policy_map.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'cloud_policy_code_generate',
        'cloud_policy_proto_compile',
        'cloud_policy_backend_header_compile',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'policy_win64',
          'type': '<(library)',
          'hard_dependency': 1,
          'sources': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
            '<(configuration_policy_type_header_path)',
          ],
          'direct_dependent_settings':  {
            'include_dirs': [
              '<(policy_out_dir)'
            ],
          },
          'dependencies': [
            'cloud_policy_code_generate',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
