# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_core',
      'type': '<(component)',
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'DEVICE_CORE_IMPLEMENTATION',
      ],
      'sources': [
        'device_client.cc',
        'device_client.h',
        'device_monitor_win.cc',
        'device_monitor_win.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ]
    },
  ],
}
