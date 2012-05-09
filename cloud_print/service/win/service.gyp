# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
    },
    'include_dirs': [
      '../../..',
    ],
  },
  'targets': [
    {
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'sources': [
        'cloud_print_service.cc',
        'cloud_print_service.h',
        'cloud_print_service.rc',
        'resource.h',
      ],
      'dependencies': [
        '../../../base/base.gyp:base', 
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
          'UACExecutionLevel': '2', # /level='requireAdministrator'
        },
      },
    },
  ],
}
