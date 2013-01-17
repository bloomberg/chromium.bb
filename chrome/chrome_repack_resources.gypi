# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_resources',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/net_internals_resources.pak',
      '<(grit_out_dir)/signin_internals_resources.pak',
      '<(grit_out_dir)/sync_internals_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/webui_resources.pak',
    ],
    'conditions': [
      ['OS != "ios" and OS != "android"', {
        # New paks should be added here by default.
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
          '<(grit_out_dir)/component_extension_resources.pak',
          '<(grit_out_dir)/options_resources.pak',
          '<(grit_out_dir)/quota_internals_resources.pak',
        ],
      }],
      ['OS != "ios"', {
        'pak_inputs': [
          '<(grit_out_dir)/devtools_discovery_page_resources.pak',
        ],
      }],
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
