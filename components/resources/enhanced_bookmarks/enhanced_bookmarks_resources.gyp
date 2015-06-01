# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'enhanced_bookmarks_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/resources/enhanced_bookmarks',
        'get_salient_image_url_gen_js': '<(SHARED_INTERMEDIATE_DIR)/closure/components/resources/enhanced_bookmarks/get_salient_image_url.js',
        'dom_initializer_gen_js': '<(SHARED_INTERMEDIATE_DIR)/closure/components/resources/enhanced_bookmarks/dom_initializer.js',
      },
      'actions': [
        {
          'action_name': 'generate_enhanced_bookmarks_resources',
          'variables': {
            'grit_grd_file': 'enhanced_bookmarks_resources.grd',
            'grit_additional_defines': [
              '-E', 'get_salient_image_url_gen_js=<(get_salient_image_url_gen_js)',
              '-E', 'dom_initializer_gen_js=<(dom_initializer_gen_js)',
            ],
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ],
}
