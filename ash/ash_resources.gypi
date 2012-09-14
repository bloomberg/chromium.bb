# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ash_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/ash_resources',
      },
      'actions': [
        {
          'action_name': 'ash_resources',
          'variables': {
            'grit_grd_file': 'resources/ash_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/ash/ash_resources',
        ],
      },
    },
    {
      'target_name': 'ash_wallpaper_resources',
      'type': 'none',
      'conditions': [
        ['use_ash==1', {
          'variables': {
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/ash_resources',
          },
          'actions': [
            {
              'action_name': 'ash_wallpapers_resources',
              'variables': {
                'grit_grd_file': 'resources/ash_wallpaper_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
          ],
          'includes': [ '../build/grit_target.gypi' ],
        }],
      ],
    },
  ],
}
