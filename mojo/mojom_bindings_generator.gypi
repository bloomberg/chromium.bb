# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojom_bindings_generator_variables.gypi',
  ],
  'rules': [
    {
      'rule_name': '<(_target_name)_mojom_bindings_generator',
      'extension': 'mojom',
      'variables': {
        'mojom_base_output_dir':
            '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'mojom_import_args%': [
         '-I<(DEPTH)'
        ],
      },
      'inputs': [
        '<@(mojom_bindings_generator_sources)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.js',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)_mojom.py',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-internal.h',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        './<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
        '--use_bundled_pylibs',
        '-d', '<(DEPTH)',
        '<@(mojom_import_args)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)',
        '--java_output_directory=<(java_out_dir)',
      ],
      'message': 'Generating Mojo bindings from <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
      'process_outputs_as_sources': 1,
    }
  ],
  'include_dirs': [
    '<(DEPTH)',
    '<(SHARED_INTERMEDIATE_DIR)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'variables': {
      'generated_src_dirs': [
        '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
      ],
    },
  },
  'hard_dependency': 1,
}
