# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_theme_resources_large',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/theme_resources_large.pak',
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(INTERMEDIATE_DIR)/repack/theme_resources_large.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)',
             '<@(pak_inputs)'],
  'process_outputs_as_mac_bundle_resources': 1,
}
