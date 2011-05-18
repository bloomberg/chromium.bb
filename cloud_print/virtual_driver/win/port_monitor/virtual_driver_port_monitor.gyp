# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
    },
    'include_dirs': [
      '../..',
    ],
    'libraries': [
      'userenv.lib',
    ],
    'sources': [
      '../virtual_driver_consts.h',
      '../virtual_driver_consts.cc',
      '../virtual_driver_helpers.h',
      '../virtual_driver_helpers.cc',
      '../virtual_driver_common_resources.rc',
      'port_monitor.cc',
      'port_monitor.h',
      'port_monitor.def',
    ],
  },
  'targets' : [
    {
      'target_name': 'gcp_portmon',
      'type': 'loadable_module',
      'dependencies': [
        '../../../../base/base.gyp:base',
      ],
      'msvs_guid': 'ED3D7186-C94E-4D8B-A8E7-B7260F638F46',
    },
    {
      'target_name': 'gcp_portmon64',
      'type': 'loadable_module',
      'defines': [
        '<@(nacl_win64_defines)',
      ],
      'dependencies': [
        '../../../../base/base.gyp:base_nacl_win64',
      ],
      'msvs_guid': '9BB292F4-6104-495A-B415-C3E314F46D6F',
      'configurations': {
        'Common_Base': {
          'msvs_target_platform': 'x64',
        },
      },
    },
    {
      'target_name': 'virtual_driver_unittests',
      'type': 'executable',
      'msvs_guid': '97F82D29-58D8-4909-86C8-F2BBBCC4FEBF',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../base/base.gyp:test_support_base',
        '../../../../testing/gmock.gyp:gmock',
        '../../../../testing/gtest.gyp:gtest',
      ],
      'sources': [
      # Infrastructure files.
        '../../../../base/test/run_all_unittests.cc',
        'port_monitor_unittest.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
