# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_chrome_resources_200_percent',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/renderer_resources_200_percent.pak',
      '<(grit_out_dir)/theme_resources_200_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_200_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_200_percent.pak',
    ],
    'conditions': [
      ['use_aura==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ash/ash_resources/ash_resources_200_percent.pak',
        ],
      }],
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
