# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ash/wm/common/resources
      'target_name': 'ash_wm_common_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/wm/common/resources',
      },
      'actions': [
        {
          'action_name': 'ash_wm_common_resources',
          'variables': {
            'grit_grd_file': 'resources/ash_wm_common_resources.grd',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ],
}
