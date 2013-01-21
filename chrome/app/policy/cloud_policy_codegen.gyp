# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    'protobuf_decoder_path':
        '<(policy_out_dir)/policy/cloud_policy_generated.cc',
    # This is the "full" protobuf, which defines one protobuf message per
    # policy. It is also the format currently used by the server.
    'chrome_settings_proto_path':
        '<(policy_out_dir)/policy/chrome_settings.proto',
    # This protobuf is equivalent to chrome_settings.proto but shares messages
    # for policies of the same type, so that less classes have to be generated
    # and compiled.
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
            '<(protobuf_decoder_path)',
            '<(chrome_settings_proto_path)',
            '<(cloud_policy_proto_path)',
          ],
          'action_name': 'generate_policy_source',
          'action': [
            'python',
            '<@(generate_policy_source_script_path)',
            '--policy-constants-header=<(policy_constant_header_path)',
            '--policy-constants-source=<(policy_constant_source_path)',
            '--chrome-settings-protobuf=<(chrome_settings_proto_path)',
            '--cloud-policy-protobuf=<(cloud_policy_proto_path)',
            '--cloud-policy-decoder=<(protobuf_decoder_path)',
            '<(OS)',
            '<(chromeos)',
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
      'type': 'static_library',
      'sources': [
        '<(cloud_policy_proto_path)',
      ],
      'variables': {
        'proto_in_dir': '<(policy_out_dir)/policy',
        'proto_out_dir': '<(proto_path_substr)',
      },
      'dependencies': [
        'cloud_policy_code_generate',
      ],
      'includes': [ '../../../build/protoc.gypi' ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # This target builds the "full" protobuf, used for tests only.
      'target_name': 'chrome_settings_proto_compile',
      'type': 'static_library',
      'sources': [
        '<(chrome_settings_proto_path)',
      ],
      'variables': {
        'proto_in_dir': '<(policy_out_dir)/policy',
        'proto_out_dir': '<(proto_path_substr)',
      },
      'dependencies': [
        'cloud_policy_code_generate',
        'cloud_policy_proto_compile',
      ],
      'includes': [ '../../../build/protoc.gypi' ],
    },
    {
      'target_name': 'policy_proto_compile',
      'type': 'static_library',
      'sources': [
        '<(proto_rel_path)/chrome_device_policy.proto',
        '<(proto_rel_path)/chrome_extension_policy.proto',
        '<(proto_rel_path)/device_management_backend.proto',
        '<(proto_rel_path)/device_management_local.proto',
        '<(proto_rel_path)/install_attributes.proto',
        '<(proto_rel_path)/old_generic_format.proto',
      ],
      'variables': {
        'proto_in_dir': '<(proto_rel_path)',
        'proto_out_dir': '<(proto_path_substr)',
      },
      'dependencies': [
        'cloud_policy_code_generate',
      ],
      'includes': [ '../../../build/protoc.gypi' ],
    },
    {
      'target_name': 'policy',
      'type': 'static_library',
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
        '<(protobuf_decoder_path)',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'cloud_policy_code_generate',
        'cloud_policy_proto_compile',
        'policy_proto_compile',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'policy_test_support',
      'type': 'none',
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(policy_out_dir)',
          '<(protoc_out_dir)',
        ],
      },
      'dependencies': [
        'policy',
        'chrome_settings_proto_compile',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'policy_win64',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(policy_constant_header_path)',
            '<(policy_constant_source_path)',
          ],
          'include_dirs': [
            '<(DEPTH)',
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
