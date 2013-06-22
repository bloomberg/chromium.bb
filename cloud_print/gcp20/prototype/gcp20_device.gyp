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
      'target_name': 'gcp20_device_lib',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'dns_packet_parser.cc',
        'dns_packet_parser.h',
        'dns_response_builder.cc',
        'dns_response_builder.h',
        'dns_sd_server.cc',
        'dns_sd_server.h',
        'gcp20_device.cc',
        'printer.cc',
        'printer.h',
        'service_parameters.cc',
        'service_parameters.h',
      ],
    },
    {
      'target_name': 'gcp20_device',
      'type': 'executable',
      'dependencies': [
        'gcp20_device.gyp:gcp20_device_lib',
      ],
      'sources': [        
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
