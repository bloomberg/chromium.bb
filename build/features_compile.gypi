# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # When including this gypi, the following variables must be set:
    #   schema_files: a list of json or IDL files that comprise the features model.
    #   cc_dir: path to generated files
    #   root_namespace: the C++ namespace that all generated files go under
    'feature_gen_dir': '<(DEPTH)/tools/json_schema_compiler',
    'feature_gen': '<(feature_gen_dir)/features_compiler.py',
  },
  'rules': [
    {
      'rule_name': 'genfeature',
      'msvs_external_rule': 1,
      'extension': 'json',
      'inputs': [
        '<(feature_gen_dir)/features_cc_generator.py',
        '<(feature_gen_dir)/code.py',
        '<(feature_gen_dir)/features_compiler.py',
        '<(feature_gen_dir)/cpp_util.py',
        '<(feature_gen_dir)/features_h_generator.py',
        '<(feature_gen_dir)/json_schema.py',
        '<(feature_gen_dir)/model.py',
        '<(feature_gen_dir)/util.cc',
        '<(feature_gen_dir)/util.h',
        '<(feature_gen_dir)/util_cc_helper.py',
        '<@(schema_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/<(RULE_INPUT_ROOT).cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(cc_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        'python',
        '<(feature_gen)',
        '<(RULE_INPUT_PATH)',
        '--root=<(DEPTH)',
        '--destdir=<(SHARED_INTERMEDIATE_DIR)',
        '--namespace=<(root_namespace)',
      ],
      'message': 'Generating C++ feature code from <(RULE_INPUT_PATH) json files',
      'process_outputs_as_sources': 1,
    },
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(DEPTH)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)',
    ]
  },
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
