# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Some plaform want to override part of the version number generation
    # (for example iOS uses a different value for PATCH level for canary).
    # This can be done settings "extra_version_path" variable to the path
    # of a file with the corresponding value overrides. If present it will
    # be loaded after all other input files.
    'extra_version_name': '',
    'conditions': [
      ['branding == "Chrome"', {
        'use_unofficial_version_number%': 0,
      }, {
        'use_unofficial_version_number%': 1,
      }],
    ],
  },
  'targets': [
    {
      # GN version: //components/version_info
      'target_name': 'version_info',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'components_strings.gyp:components_strings',
        'generate_version_info',
      ],
      'sources': [
        'version_info/version_info.cc',
        'version_info/version_info.h',
      ],
      'conditions': [
        ['use_unofficial_version_number==1', {
          'dependencies': [
              '../ui/base/ui_base.gyp:ui_base',
            ],
          'defines': ['USE_UNOFFICIAL_VERSION_NUMBER'],
        }],
      ],
    },
    {
      # GN version: //components/version_info:generate_version
      'target_name': 'generate_version_info',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      # Because generate_version_info generates a header, the target must set
      # the hard_dependency flag.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generation_version_info',
          'message': 'Generating version information',
          'variables': {
            'extra_version_flags': [],
            'lastchange_path': '../build/util/LASTCHANGE',
            'version_py_path': '../build/util/version.py',
            'template_input_path': 'version_info/version_info_values.h.version',
            # Use VERSION and BRANDING files from //chrome even if this is bad
            # dependency until they are moved to src/ for VERSION and to the
            # version_info component for BRANDING. Synchronisation with TPM and
            # all release script is required for thoses moves. They are tracked
            # by issues http://crbug.com/512347 and http://crbug.com/513603.
            'version_path': '../chrome/VERSION',
            'branding_path': '../chrome/app/theme/<(branding_path_component)/BRANDING',
          },
          'inputs': [
            '<(version_py_path)',
            '<(template_input_path)',
            '<(version_path)',
            '<(branding_path)',
            '<(lastchange_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/components/version_info/version_info_values.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(branding_path)',
            '-f', '<(lastchange_path)',
            '<@(extra_version_flags)',
            '<(template_input_path)',
            '<@(_outputs)',
          ],
          'conditions': [
            ['extra_version_name!=""', {
              'variables': {
                'extra_version_flags': [
                  '-f', '<(extra_version_name)',
                ],
              },
              'inputs': [
                '<(extra_version_name)'
              ],
            }],
          ],
        },
      ],
    },
  ],
  'conditions': [
    ['OS=="ios"', {
      'variables': {
        # Use nested 'variables' to workaround how variables work with gyp (no
        # determined ordering and thuse it is not possible to define a variable
        # in function of another).
        'variables': {
          # Path to the file used to override the version PATH level on iOS.
          # Default to ios/build/util/VERSION.
          'ios_extra_version_path%': '../ios/build/util/VERSION',
        },
        'extra_version_name': '<(ios_extra_version_path)'
      },
    }],
  ],
}
