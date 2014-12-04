# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome/browser/devtools:webrtc_device_provider_resources
      'target_name': 'webrtc_device_provider_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
      },
      'actions': [
        {
          'action_name': 'generate_webrtc_device_provider_resources',
          'variables': {
            'grit_grd_file': 'device/webrtc/resources.grd',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ]
}
