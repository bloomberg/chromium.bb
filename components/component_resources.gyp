# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
  },
  'targets': [
    {
      'target_name': 'component_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'component_resources',
          'variables': {
            'grit_grd_file': 'component_resources.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/component_resources',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
     'direct_dependent_settings': {
        'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/component_resources',
          ],
        },
    },
  ],
}
