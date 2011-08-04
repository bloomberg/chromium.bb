# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
    '../..'
    ],
    'variables': { 
      'chromium_code': 1
    } 
  },
  'targets' : [
    {
      'target_name': 'GCP-driver',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'sources' : [
        'virtual_driver_posix.cc',
        'printer_driver_util_linux.cc',
        'printer_driver_util_posix.h',
        '../virtual_driver_switches.cc',
        '../virtual_driver_switches.h',
      ]
    },
  ],
}
