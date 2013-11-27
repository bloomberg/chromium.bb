# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_resources',
  'variables': {
    'pak_inputs': [
      '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/components/dom_distiller_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/webui_resources.pak',
      '<(grit_out_dir)/browser_resources.pak',
      '<(grit_out_dir)/common_resources.pak',
      '<(grit_out_dir)/memory_internals_resources.pak',
      '<(grit_out_dir)/net_internals_resources.pak',
      '<(grit_out_dir)/signin_internals_resources.pak',
      '<(grit_out_dir)/sync_internals_resources.pak',
      '<(grit_out_dir)/translate_internals_resources.pak',
    ],
    'conditions': [
      ['OS != "ios"', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.pak',
          '<(grit_out_dir)/devtools_discovery_page_resources.pak',
        ],
      }],
      ['OS != "ios" and OS != "android"', {
        # New paks should be added here by default.
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
          '<(grit_out_dir)/component_extension_resources.pak',
          '<(grit_out_dir)/options_resources.pak',
          '<(grit_out_dir)/quota_internals_resources.pak',
          '<(grit_out_dir)/sync_file_system_internals_resources.pak',
        ],
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
    '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
