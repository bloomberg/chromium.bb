# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'browser_ui_views',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/ui.gyp:ui',
      ],
      'defines': [
        'CHROME_VIEWS_IMPLEMENTATION',
      ],
      'sources': [
        'browser/ui/views/accelerator_table.cc',
        'browser/ui/views/accelerator_table.h',
        'browser/ui/views/event_utils.cc',
        'browser/ui/views/event_utils.h',
      ],
    },
  ],
}
