# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_base_dir': '<(SHARED_INTERMEDIATE_DIR)',
    'grit_out_dir': '<(grit_base_dir)/ios/chrome',
  },
  'targets': [
    {
      'target_name': 'ios_chrome_resources',
      'type': 'none',
      'dependencies': [
        'ios_strings_resources_gen',
        'ios_theme_resources_gen',
      ],
    },
    {
      'target_name': 'ios_strings_resources_gen',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'ios_strings_resources',
          'variables': {
            'grit_whitelist': '',
            'grit_grd_file': 'app/strings/ios_strings_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      # Override the exported include-dirs; ios_strings_resources.h should only
      # be referenceable as ios/chrome/grit/ to allow DEPS-time checking of
      # usage.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_dir)',
        ],
        'include_dirs!': [
          '<(grit_out_dir)',
        ],
      }
    },
    {
      'target_name': 'ios_theme_resources_gen',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'ios_theme_resources',
          'variables': {
            # TODO(lliabraa): Remove this whitelist.
            'grit_whitelist': '<(DEPTH)/ios/build/grit_whitelist.txt',
            'grit_grd_file': 'app/theme/ios_theme_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      # Override the exported include-dirs; ios_theme_resources.h should only be
      # referencable as ios/chrome/grit/ to allow DEPS-time checking of usage.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_dir)',
        ],
        'include_dirs!': [
          '<(grit_out_dir)',
        ],
      },
    },
  ],
}

