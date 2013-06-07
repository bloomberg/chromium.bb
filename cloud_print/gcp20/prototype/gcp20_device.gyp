# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
      'enable_wexit_time_destructors': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
      # To allow including "version.h"
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
  },
  'targets': [
    {
      'target_name': 'gcp20_device',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'dns_sd_server.cc',
        'gcp20_device.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'AdditionalDependencies': [
              'secur32.lib',
              'httpapi.lib',
              'Ws2_32.lib',
          ],
        },
      },
    },
  ],
}
