# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate localized strings.xml from a grd file.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_strings_grd',
#   'type': 'none',
#   'variables': {
#     'grd_file': 'path/to/grd/file',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  grd_file - The path to the grd file to use.
{
  'variables': {
    'intermediate_dir': '<(PRODUCT_DIR)/<(_target_name)',
    'res_grit_dir': '<(intermediate_dir)/res_grit',
    'grit_grd_file': '<(grd_file)',
    'resource_input_paths': [
      '<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(res_grit_dir)" <(grd_file))'
    ],
  },
  'all_dependent_settings': {
    'variables': {
      'additional_res_dirs': ['<@(res_grit_dir)'],
      'dependencies_res_files': ['<@(resource_input_paths)'],
      'dependencies_res_input_dirs': ['<@(res_grit_dir)'],
    },
  },
  'actions': [
    {
      'action_name': 'generate_localized_strings_xml',
      'variables': {
        'grit_additional_defines': ['-E', 'ANDROID_JAVA_TAGGED_ONLY=false'],
        'grit_out_dir': '<(res_grit_dir)',
        # resource_ids is unneeded since we don't generate .h headers.
        'grit_resource_ids': '',
      },
      'includes': ['../build/grit_action.gypi'],
    },
  ],
}
