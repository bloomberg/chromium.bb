# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
    },
    'include_dirs': [
      '../../../..',
    ],
  },
  'targets' : [
    {
      'target_name': 'virtual_driver_setup',
      'type': 'executable',
      'msvs_guid': 'E1E25ACA-043D-4D6E-A06F-97126532843A',
      'dependencies': [
        '../../../../base/base.gyp:base',
        'virtual_driver_setup_resources',
      ],
      'sources': [
        'setup.cc',
        '../virtual_driver_consts.h',
        '../virtual_driver_consts.cc',
        '../virtual_driver_helpers.h',
        '../virtual_driver_helpers.cc',
        '../virtual_driver_common_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/virtual_driver_setup_resources/virtual_driver_setup_resources_en-US.rc',
        '<(SHARED_INTERMEDIATE_DIR)/virtual_driver_setup_resources/virtual_driver_setup_resources_es.rc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
        },
      },
    },
    {
      'target_name': 'virtual_driver_setup_resources',
      'type': 'none',
      'msvs_guid': '13D30B4C-1B06-40DB-8ED6-D2CAD76940CB',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/virtual_driver_setup_resources',
      },
      'actions': [
        {
          'action_name': 'virtual_driver_setup_resources',
          'variables': {
            'grit_grd_file': 'virtual_driver_setup_resources.grd',
          },
          'includes': [ '../../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../../build/grit_target.gypi' ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

