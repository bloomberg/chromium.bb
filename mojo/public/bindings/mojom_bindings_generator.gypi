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
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_proxy_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_stub_case',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_stub_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/interface_stub_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/module.cc-template',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/module.h-template',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/module_internal.h-template',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/params_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/params_serialization',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/proxy_implementation',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_builder_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_serialization',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_serialization_definition',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/struct_serialization_traits',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/template_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/cpp_templates/wrapper_class_declaration',
        '<(DEPTH)/mojo/public/bindings/generators/js_templates/module.js.tmpl',
        '<(DEPTH)/mojo/public/bindings/generators/mojom.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_cpp_generator.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_data.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_generator.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_js_generator.py',
        '<(DEPTH)/mojo/public/bindings/generators/mojom_pack.py',
        '<(DEPTH)/mojo/public/bindings/generators/template_expander.py',
      ],
      'outputs': [
        '<(output_dir)/<(RULE_INPUT_ROOT).cc',
        '<(output_dir)/<(RULE_INPUT_ROOT).h',
        '<(output_dir)/<(RULE_INPUT_ROOT).js',
        '<(output_dir)/<(RULE_INPUT_ROOT)_internal.h',
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
  'dependencies': [
    'mojo_bindings',
    'mojo_system',
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
