# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'wifi_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:onc_component',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'WIFI_IMPLEMENTATION',
      ],
      'sources': [
        'wifi/wifi_export.h',
        'wifi/wifi_service.cc',
        'wifi/wifi_service.h',
        'wifi/fake_wifi_service.cc',
        'wifi/wifi_service_win.cc',
      ],
      'conditions': [
        ['OS == "win"', {
          'link_settings': {
            'libraries': [
              '-liphlpapi.lib',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'wifi_test',
      'type': 'executable',
      'dependencies': [
        'wifi_component',
        '../base/base.gyp:base',
        '../components/components.gyp:onc_component',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'wifi/wifi_test.cc',
      ],
    },
  ],
}
