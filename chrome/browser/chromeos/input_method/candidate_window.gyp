# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'candidate_window',
      'type': 'executable',
      'dependencies': [
        '../../../../app/app.gyp:app_strings',
        '../../../../base/base.gyp:base',
        '../../../../build/linux/system.gyp:gtk',
        '../../../../build/linux/system.gyp:x11',
        '../../../../chrome/chrome.gyp:common_constants',
        '../../../../skia/skia.gyp:skia',
        '../../../../views/views.gyp:views',
      ],
      'sources': [
        'candidate_window.h',
        'candidate_window.cc',
        'candidate_window_main.cc',
        # For loading libcros.
        '../cros/cros_library_loader.cc',
      ],
      'conditions': [
        ['system_libcros==0', {
          'dependencies': [
            '../../../../third_party/cros/cros_api.gyp:cros_api',
          ],
          'include_dirs': [
            '../../../../third_party/',
          ],
        }],
        ['system_libcros==1', {
          'link_settings': {
            'libraries': [
              '-lcrosapi',
            ],
          },
        }],
      ],
    },
  ],
}
