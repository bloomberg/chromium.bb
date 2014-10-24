# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_web',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../ui/base/ui_base.gyp:ui_base',
        '../../ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'navigation/navigation_item_impl.h',
        'navigation/navigation_item_impl.mm',
        'public/favicon_status.cc',
        'public/favicon_status.h',
        'public/navigation_item.h',
        'public/referrer.h',
        'public/security_style.h',
        'public/ssl_status.cc',
        'public/ssl_status.h',
        'public/user_agent.h',
        'public/user_agent.mm',
      ],
    },
  ],
}
