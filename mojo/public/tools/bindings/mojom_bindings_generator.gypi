# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'rules': [
    {
      'rule_name': 'Generate C++ source files from mojom files',
      'extension': 'mojom',
      'variables': {
        # TODO(sky): uncomment this once gyp bug fixed and remove explicit
        # setting everywhere
        # 'mojom_base_output_dir':
        #      '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
        'mojom_bindings_generator':
            '<(DEPTH)/mojo/public/tools/bindings/mojom_bindings_generator.py',
      },
      'inputs': [
        '<(mojom_bindings_generator)',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/enum_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_macros.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_proxy_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_stub_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module.cc.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module.h.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module-internal.h.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/params_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_builder_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_destructor.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_macros.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/wrapper_class_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/enum_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/interface_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/module.js.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/struct_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/mojom_cpp_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/generators/mojom_js_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/parse/__init__.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/parse/mojo_lexer.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/parse/mojo_parser.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/parse/mojo_translate.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/__init__.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/mojom.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/mojom_data.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/mojom_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/mojom_pack.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/generate/template_expander.py',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.js',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-internal.h',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        '<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
        '-d', '<(DEPTH)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)',
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
  },
  'hard_dependency': 1,
}
