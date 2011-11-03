# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_pseudo_locales',
  'variables': {
    'conditions': [
      ['branding=="Chrome"', {
        'branding_flag': ['-b', 'google_chrome',],
      }, {  # else: branding!="Chrome"
        'branding_flag': ['-b', 'chromium',],
      }],
    ]
  },
  'inputs': [
    'tools/build/repack_locales.py',
    '<!@pymod_do_main(repack_locales -i <(branding_flag) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(pseudo_locales))'
  ],
  'conditions': [
    ['OS == "mac"', {
      'outputs': [
        '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(pseudo_locales))'
      ],
    }, { # else 'OS != "mac"'
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(pseudo_locales).pak'
      ],
    }],
  ],
  'action': [
    '<@(repack_locales_cmd)',
    '<@(branding_flag)',
    '-g', '<(grit_out_dir)',
    '-s', '<(SHARED_INTERMEDIATE_DIR)',
    '-x', '<(SHARED_INTERMEDIATE_DIR)/.',
    '<@(pseudo_locales)',
  ],
}
