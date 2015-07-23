# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/secure_display
      'target_name': 'secure_display',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../ui/gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'SECURE_DISPLAY_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'secure_display/elide_url.h',
        'secure_display/elide_url.cc',
      ]
    }
  ]
}

