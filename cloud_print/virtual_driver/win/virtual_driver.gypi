# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)',
    ],
    'libraries': [
      'userenv.lib',
    ],
  },
  'targets' : [
    {
      'target_name': 'virtual_driver_lib<(virtual_driver_suffix)',
      'type': 'static_library',
      'sources': [
        '../virtual_driver_switches.cc',
        '../virtual_driver_switches.h',
        'virtual_driver_consts.cc',
        'virtual_driver_consts.h',
        'virtual_driver_helpers.cc',
        'virtual_driver_helpers.h',
      ],
    },
    {
      'target_name': 'gcp_portmon_lib<(virtual_driver_suffix)',
      'type': 'static_library',
      'sources': [
        'port_monitor/port_monitor.cc',
        'port_monitor/port_monitor.h',
      ],
      'dependencies': [
        'virtual_driver_lib<(virtual_driver_suffix)',
      ],
    },
    {
      'target_name': 'gcp_portmon<(virtual_driver_suffix)',
      'type': 'loadable_module',
      'sources': [
        'port_monitor/port_monitor.def',
        'port_monitor/port_monitor_dll.cc',
        'virtual_driver_common_resources.rc',
      ],
      'dependencies': [
        'gcp_portmon_lib<(virtual_driver_suffix)',
      ],
    },
  ],
}
