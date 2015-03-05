# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file included into a targets definition creates a target that generates
# native and HTML/JS supporting code for Web UI element from element's
# declaration JSON file.
#
# Example:
# 'targets': [
#   ...
#   {
#     'variables': {
#       'declaration_file': 'path/to/file.json'
#     }
#     'includes': ['path/to/this/file.gypi']
#   },
#   ...
# ]
#
# Such inclusion creates a target, which name is deduced from declaration file
# name by removing extension and adding '_wug_generated' prefix, e.g. for
# declaration file named 'some_type.json' will be created target named
# 'some_type_wug_generated'. This is needed to properly handle dependencies
# between declaration files and their imports.
#
# For declaration file with a full path 'src/full/path/some_type.json' 5 files
# will be generated and compiled:
#   <(SHARED_INTERMEDIATE_DIR)/wug/full/path/some_type_export.h
#   <(SHARED_INTERMEDIATE_DIR)/wug/full/path/some_type_view.h
#   <(SHARED_INTERMEDIATE_DIR)/wug/full/path/some_type_view_model.cc
#   <(SHARED_INTERMEDIATE_DIR)/wug/full/path/some_type_view_model.h
#   <(SHARED_INTERMEDIATE_DIR)/wug/full/path/some_type_webui_view.cc

{
  'variables': {
    'generator_dir': '<(DEPTH)/components/webui_generator/generator',
    'helper_path': '<(generator_dir)/build_helper.py',
    'generator_path': '<(generator_dir)/gen_sources.py',
    'out_dir': '<(SHARED_INTERMEDIATE_DIR)/wug',
    'helper_cl': 'python <(helper_path) --root=<(DEPTH) <(declaration_file)',
    'dirname': '<(out_dir)/<!(<(helper_cl) --output=dirname)',
    'view_cc': '<(dirname)/<!(<(helper_cl) --output=view_cc)',
    'view_h': '<(dirname)/<!(<(helper_cl) --output=view_h)',
    'model_cc': '<(dirname)/<!(<(helper_cl) --output=model_cc)',
    'model_h': '<(dirname)/<!(<(helper_cl) --output=model_h)',
    'export_h': '<(dirname)/<!(<(helper_cl) --output=export_h)',
    'impl_macro': '<!(<(helper_cl) --output=impl_macro)',
  },
  'target_name': '<!(<(helper_cl) --output=target_name)',
  'type': '<(component)',
  'sources': [
    '<(view_cc)',
    '<(view_h)',
    '<(model_cc)',
    '<(model_h)',
    '<(export_h)',
  ],
  'defines': [
    '<(impl_macro)',
  ],
  'actions': [
    {
      'action_name': 'gen_files',
      'inputs': [
        '<(generator_path)',
        '<(generator_dir)/declaration.py',
        '<(generator_dir)/export_h.py',
        '<(generator_dir)/html_view.py',
        '<(generator_dir)/util.py',
        '<(generator_dir)/view_model.py',
        '<(generator_dir)/web_ui_view.py',
        '<(declaration_file)',
      ],
      'outputs': [
        '<(view_cc)',
        '<(view_h)',
        '<(model_cc)',
        '<(model_h)',
        '<(export_h)',
      ],
      'action': [
        'python',
        '<(generator_path)',
        '--root=<(DEPTH)',
        '--destination=<(out_dir)',
        '<(declaration_file)'
      ],
      'message': 'Generating C++ code from <(declaration_file).',
    },
  ],
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base',
    '<(DEPTH)/components/components.gyp:login',
    '<(DEPTH)/components/components.gyp:webui_generator',
    '<(DEPTH)/components/components_strings.gyp:components_strings',
    '<(DEPTH)/skia/skia.gyp:skia',
    '<!@(<(helper_cl) --output=import_dependencies)',
  ],
  'include_dirs': [
    '<(DEPTH)',
    '<(out_dir)',
  ],
  'all_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)',
      '<(out_dir)',
    ],
  },
  'export_dependent_settings': [
    '<(DEPTH)/components/components.gyp:webui_generator',
    '<(DEPTH)/components/components.gyp:login',
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
