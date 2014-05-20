# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_lib',
      'type': '<(component)',
      'dependencies': [
        '../ui/aura/aura.gyp:aura',
        '../ui/views/views.gyp:views',
        '../ui/accessibility/accessibility.gyp:ax_gen',
        '../skia/skia.gyp:skia',
      ],
      'defines': [
        'ATHENA_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under athena, except unittests
        'athena_export.h',
        'screen/background_controller.h',
        'screen/background_controller.cc',
        'screen/public/screen_manager.h',
        'screen/screen_manager_impl.cc',
        'wm/public/window_manager.h',
        'wm/window_manager_impl.cc',
      ],
    },
  ],
}

