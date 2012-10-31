# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_locales',
  'variables': {
    'conditions': [
      ['branding=="Chrome"', {
        'branding_flag': ['-b', 'google_chrome',],
      }, {  # else: branding!="Chrome"
        'branding_flag': ['-b', 'chromium',],
      }],
    ],
    'repack_extra_flags%': [],
    'repack_output_dir%': '<(SHARED_INTERMEDIATE_DIR)',
  },
  'inputs': [
    'tools/build/repack_locales.py',
    '<!@pymod_do_main(repack_locales -i -p <(OS) <(branding_flag) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(repack_output_dir) <(repack_extra_flags) <(locales))'
  ],
  'outputs': [
    '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(repack_output_dir) <(locales))'
  ],
  'action': [
    '<@(repack_locales_cmd)',
    '<@(branding_flag)',
    '-p', '<(OS)',
    '-g', '<(grit_out_dir)',
    '-s', '<(SHARED_INTERMEDIATE_DIR)',
    '-x', '<(repack_output_dir)/.',
    '<@(repack_extra_flags)',
    '<@(locales)',
  ],
}
