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
        'content_browser',
        'content_common',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../crypto/crypto.gyp:crypto',
        '../ui/ui.gyp:ui',
      ],
      'sources': [
        'app/content_main.cc',
        'app/content_main.h',
        'app/content_main_delegate.cc',
        'app/content_main_delegate.h',
        'app/startup_helper_win.cc',
        'app/startup_helper_win.h',
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
