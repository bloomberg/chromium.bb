# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'component_strings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'component_strings',
          'variables': {
            'grit_grd_file': 'component_strings.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/components/strings',
        ],
      },
      'hard_dependency': 1,
    },
  ],
}
