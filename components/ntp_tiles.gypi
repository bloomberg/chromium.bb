# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ntp_tiles
      'target_name': 'ntp_tiles',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'ntp_tiles/pref_names.h',
        'ntp_tiles/pref_names.cc',
        'ntp_tiles/switches.h',
        'ntp_tiles/switches.cc',
      ],
    },
  ],
}
