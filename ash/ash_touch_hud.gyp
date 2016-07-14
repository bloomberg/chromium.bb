# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # These files lists are shared with the GN build.
    'touch_hud_sources': [
      'touch_hud/ash_touch_hud_export.h',
      'touch_hud/touch_hud_renderer.cc',
      'touch_hud/touch_hud_renderer.h',
     ],
  },
  'targets': [
    {
      # GN version: //ash/touch_hud
      'target_name': 'ash_touch_hud',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../ui/compositor/compositor.gyp:compositor',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/views/views.gyp:views',
      ],
      'defines': [
        'ASH_TOUCH_HUD_IMPLEMENTATION',
      ],
      'sources': [
        '<@(touch_hud_sources)',
      ],
    },
  ],
}
