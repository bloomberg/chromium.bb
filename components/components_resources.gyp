# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/resources
      'target_name': 'components_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components',
      },
      'actions': [
        {
          # GN version: //components/resources:components_resources
          'action_name': 'generate_components_resources',
          'variables': {
            'grit_grd_file': 'resources/components_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          # GN version: //components/resources:components_scaled_resources
          'action_name': 'generate_components_scaled_resources',
          'variables': {
            'grit_grd_file': 'resources/components_scaled_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}
