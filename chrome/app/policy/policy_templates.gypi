# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="win" or OS=="mac" or OS=="linux"', {
      'targets': [
        {
          # policy_templates has different inputs and outputs, so it can't use
          # the rules of chrome_strings
          'target_name': 'policy_templates',
          'type': 'none',
          'variables': {
            'grit_grd_file': 'policy_templates.grd',
            'grit_info_cmd': ['python', '<(DEPTH)/tools/grit/grit_info.py',
                              '<@(grit_defines)'],
          },
          'includes': [ '../../../build/grit_target.gypi' ],
          'actions': [
            {
              'action_name': 'policy_templates',
              'includes': [ '../../../build/grit_action.gypi' ],
            },
          ],
          'conditions': [
            ['OS=="win"', {
              'variables': {
                'version_path': '<(grit_out_dir)/app/policy/VERSION',
                'template_files': [
                  '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(grit_grd_file))'
                ],
              },
              'actions': [
                {
                  'action_name': 'add_version',
                  'inputs': ['../../VERSION'],
                  'outputs': ['<(version_path)'],
                  'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
                  'msvs_cygwin_shell': 1,
                },
                {
                  # Add all the templates generated at the previous step into
                  # a zip archive.
                  'action_name': 'pack_templates',
                  'variables': {
                    'zip_script':
                        'tools/build/win/make_policy_zip.py'
                  },
                  'inputs': [
                    '<(version_path)',
                    '<@(template_files)',
                    '<(zip_script)'
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/policy_templates.zip'
                  ],
                  'action': [
                    'python',
                    '<(zip_script)',
                    '--output',
                    '<@(_outputs)',
                    '--basedir', '<(grit_out_dir)/app/policy',
                    # The list of files in the destination zip is derived from
                    # the list of output nodes in the following grd file.
                    # This whole trickery is necessary because we cannot pass
                    # the entire list of file names as command line arguments,
                    # because they would exceed the length limit on Windows.
                    '--grd_input',
                    '<(grit_grd_file)',
                    '--grd_strip_path_prefix',
                    'app/policy',
                    '--extra_input',
                    'VERSION',
                    # Module to be used to process grd_input'.
                    '--grit_info',
                    '<(DEPTH)/tools/grit/grit_info.py',
                    '<@(grit_defines)',
                  ],
                  'message': 'Packing generated templates into <(_outputs)',
                  'msvs_cygwin_shell': 1,
                }
              ]
            }],
          ],  # conditions
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
                # Use plutil -convert xml1 to put the plist into Apple's
                # canonical format. As a side effect, this ensures that the
                # plist is well-formed.
                'plutil',
                '-convert',
                'xml1',
                '<@(_inputs)',
                '-o',
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
                # The reason we are not enumerating all the locales is that
                # the translations would eat up 3.5MB disk space in the
                # application bundle:
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
              'msvs_cygwin_shell': 1,
            },
          ],
        },
      ]
    }]
  ],  # 'conditions'
}
