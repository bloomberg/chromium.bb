# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/extensions',
  },
  'targets': [
    {
      'target_name': 'extensions_resources',
      'type': 'none',
      'actions': [
        # Data resources.
        {
          'action_name': 'extensions_resources',
          'variables': {
            'grit_grd_file': 'extensions_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/extensions',
        ],
      },
      'hard_dependency': 1,
    }
  ]
}
