# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_chrome',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/browser_resources.pak',
      '<(grit_out_dir)/common_resources.pak',
      '<(grit_out_dir)/renderer_resources.pak',
      '<(grit_out_dir)/theme_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
    ],
    'conditions': [
      ['OS != "mac" or use_aura==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/native_theme/native_theme_resources.pak',
        ]
      }],
      ['enable_extensions==1', {
        'pak_inputs': [
          '<(grit_out_dir)/extensions_api_resources.pak',
        ],
      }],
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
