# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_app',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'content_common',
        '../base/base.gyp:base',
      ],
      'sources': [
        'app/content_main.cc',
        'app/content_main.h',
        'app/content_main_delegate.h',
        'app/sandbox_helper_win.cc',
        'app/sandbox_helper_win.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
      ],
    },
  ],
}
