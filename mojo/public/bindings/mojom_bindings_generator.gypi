# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'output_dir': '<(SHARED_INTERMEDIATE_DIR)/mojom',
  },
  'rules': [
    {
      'rule_name': 'Generate C++ source files from mojom files',
      'extension': 'mojom',
      'variables': {
        'mojom_bindings_generator':
            '<(DEPTH)/mojo/public/bindings/mojom_bindings_generator.py',
      },
      'inputs': [
        '<(mojom_bindings_generator)',
        '<(DEPTH)/mojo/public/bindings/parse/mojo_parser.py',
        '<(DEPTH)/mojo/public/bindings/parse/mojo_translate.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_data.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_pack.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_cpp_generator.py',
      ],
      'outputs': [
        '<(output_dir)/<(RULE_INPUT_ROOT).h',
        '<(output_dir)/<(RULE_INPUT_ROOT)_internal.h',
        '<(output_dir)/<(RULE_INPUT_ROOT).cc',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        '<(RULE_INPUT_PATH)',
        '-i', 'mojom',
        '-o', '<(output_dir)',
      ],
      'message': 'Generating C++ from mojom <(RULE_INPUT_PATH)',
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
  },
  'hard_dependency': 1,
}
