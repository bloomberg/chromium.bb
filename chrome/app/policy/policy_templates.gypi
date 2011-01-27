# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'policy_out_dir': '<(SHARED_INTERMEDIATE_DIR)/policy',
    'generate_policy_source_script':
       ['python', 'tools/build/generate_policy_source.py'],
  },
  'target_defaults': {
    'variables': {
      'policy_target': 0,
    },
    'target_conditions': [
      [ 'policy_target==1', {
        'include_dirs': [
          '<(policy_out_dir)',
        ],
        'actions': [
          {
            'variables':
              {
                'policy_constant_header':
                    '<(policy_out_dir)/policy/policy_constants.h',
                'policy_constant_source':
                    '<(policy_out_dir)/policy/policy_constants.cc',
                'configuration_policy_type_header':
                    '<(policy_out_dir)/policy/configuration_policy_type.h',
              },
            'inputs': [
              'policy_templates.json',
              '../../tools/build/generate_policy_source.py'
            ],
            'outputs': [
              '<(policy_constant_header)',
              '<(policy_constant_source)',
              '<(configuration_policy_type_header)',
            ],
            'action_name': 'generate_policy_source',
            'action': [
              '<@(generate_policy_source_script)',
              '--policy-constants-header=<(policy_constant_header)',
              '--policy-constants-source=<(policy_constant_source)',
              '--policy-type-header=<(configuration_policy_type_header)',
              '<(OS)',
              '<@(_inputs)',
            ],
            'message': 'Generating policy source',
            'process_outputs_as_sources': 1,
          },
        ],
      }, ],
    ],
  },
  'targets': [
    {
      'target_name': 'policy',
      'type': '<(library)',
      'include_dirs': [
         '<(policy_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/policy'
        ],
      },
      'variables': {
        'policy_target': 1,
      },
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'policy_win64',
          'type': '<(library)',
          'variables': {
            'policy_target': 1,
          },
          'direct_dependent_settings':  {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/policy'
            ],
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="win" or OS=="mac" or OS=="linux"', {
      'targets': [
        {
          # policy_templates has different inputs and outputs, so it can't use
          # the rules of chrome_strings
          'target_name': 'policy_templates',
          'type': 'none',
          'variables': {
            'grd_path': 'policy_templates.grd',
            'template_files': [
              '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(grd_path))'
            ],
          },
          'actions': [
            {
              'action_name': 'policy_templates',
              'variables': {
                'conditions': [
                  ['branding=="Chrome"', {
                    # TODO(mmoss) The .grd files look for _google_chrome, but
                    # for consistency they should look for GOOGLE_CHROME_BUILD
                    # like C++.
                    # Clean this up when Windows moves to gyp.
                    'chrome_build': '_google_chrome',
                  }, {  # else: branding!="Chrome"
                    'chrome_build': '_chromium',
                  }],
                ],
              },
              'inputs': [
                '<!@(<(grit_info_cmd) --inputs <(grd_path))',
              ],
              'outputs': [
                '<@(template_files)'
              ],
              'action': [
                '<@(grit_cmd)',
                '-i', '<(grd_path)', 'build',
                '-o', '<(grit_out_dir)',
                '-D', '<(chrome_build)'
              ],
              'conditions': [
                ['chromeos==1', {
                  'action': ['-D', 'chromeos'],
                }],
                ['use_titlecase_in_grd_files==1', {
                  'action': ['-D', 'use_titlecase'],
                }],
                ['OS == "mac"', {
                  'action': ['-D', 'mac_bundle_id=<(mac_bundle_id)'],
                }],
              ],
              'message': 'Generating policy templates from <(grd_path)',
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(grit_out_dir)',
            ],
          },
          'conditions': [
            ['OS=="win"', {
              'actions': [
                {
                  # Add all the templates generated at the previous step into
                  # a zip archive.
                  'action_name': 'pack_templates',
                  'variables': {
                    'zip_script':
                        'tools/build/win/make_zip_with_relative_entries.py'
                  },
                  'inputs': [
                    '<@(template_files)',
                    '<(zip_script)'
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/policy_templates.zip'
                  ],
                  'action': [
                    'python',
                    '<(zip_script)',
                    '<@(_outputs)',
                    '<(grit_out_dir)/app/policy',
                    '<@(template_files)'
                  ],
                  'message': 'Packing generated templates into <(_outputs)',
                }
              ]
            }],
            ['OS=="win"', {
              'dependencies': ['../build/win/system.gyp:cygwin'],
            }],
          ],
        },
      ],  # 'targets'
    }],  # OS=="win" or OS=="mac" or OS=="linux"
    ['OS=="mac"', {
      'targets': [
        {
          # This is the bundle of the manifest file of Chrome.
          # It contains the manifest file and its string tables.
          'target_name': 'chrome_manifest_bundle',
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_extension': 'manifest',
          'product_name': '<(mac_bundle_id)',
          'variables': {
            # This avoids stripping debugging symbols from the target, which
            # would fail because there is no binary code here.
            'mac_strip': 0,
          },
          'dependencies': [
             # Provides app-Manifest.plist and its string tables:
            'policy_templates',
          ],
          'actions': [
            {
              'action_name': 'Copy MCX manifest file to manifest bundle',
              'inputs': [
                '<(grit_out_dir)/app/policy/mac/app-Manifest.plist',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/app_manifest/<(mac_bundle_id).manifest',
              ],
              'action': [
                'cp',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
              'message':
                'Copying the MCX policy manifest file to the manifest bundle',
              'process_outputs_as_mac_bundle_resources': 1,
            },
            {
              'action_name':
                'Copy Localizable.strings files to manifest bundle',
              'variables': {
                'input_path': '<(grit_out_dir)/app/policy/mac/strings',
                # Directory to collect the Localizable.strings files before
                # they are copied to the bundle.
                'output_path': '<(INTERMEDIATE_DIR)/app_manifest',
                # TODO(gfeher): replace this with <(locales) when we have real
                # translations
                'available_locales': 'en',
              },
              'inputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(input_path)/ZZLOCALE.lproj/Localizable.strings\' <(available_locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/Localizable.strings\' <(available_locales))',
              ],
              'action': [
                'cp', '-R',
                '<(input_path)/',
                '<(output_path)',
              ],
              'message':
                'Copy the Localizable.strings files to the manifest bundle',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
        },
      ]
    }]
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
