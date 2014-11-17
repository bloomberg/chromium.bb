# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
  ['OS=="win"', {
      'targets': [
          {
            'target_name': 'browser_watcher_lib',
            'type': 'static_library',
            'sources': [
              'browser_watcher/watcher_win.cc',
              'browser_watcher/watcher_win.h',
            ],
            'dependencies': [
              '../base/base.gyp:base',
            ],
          },
        ],
      }
    ],
  ],
}