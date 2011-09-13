# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
      # TODO(dpranke): Fix indentation
      'include_dirs': [
        '..',
      ],
      'dependencies': [
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
}
