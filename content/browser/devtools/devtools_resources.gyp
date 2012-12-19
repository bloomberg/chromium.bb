# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../../../..',
      },{
        'webkit_src_dir': '../../../third_party/WebKit',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'devtools_resources',
      'type': 'none',
      'dependencies': [
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:generate_devtools_grd',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'devtools_resources',
          # This can't use build/grit_action.gypi because the grd file
          # is generated at build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../../../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd',
          },
          'inputs': [
            '<(grit_grd_file)',
            '<!@pymod_do_main(grit_info --inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_resources.h',
            '<(grit_out_dir)/devtools_resources.pak',
            '<(grit_out_dir)/grit/devtools_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-f', 'GRIT_DIR/../gritsettings/resource_ids',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)' ],
          'message': 'Generating resources from <(grit_grd_file)',
          'msvs_cygwin_shell': 1,
        }
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ],
}
