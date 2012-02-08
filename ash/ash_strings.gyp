# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
  },

  'targets': [
    {
      'target_name': 'ash_strings',
      'type': 'none',
      'actions': [
        # Localizable resources.
        {
          'action_name': 'ash_strings',
          'variables': {
            'grit_grd_file': 'ash_strings.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash_strings',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/ash_strings',
        ],
      },
    },
  ],
}
