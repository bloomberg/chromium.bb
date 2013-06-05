# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'chrome_process_finder',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'policy_path_parser',
        '../base/base.gyp:base',
        '../chrome/chrome.gyp:metro_utils',
        '../chrome/common_constants.gyp:common_constants',
      ],
      'sources': [
        'browser/chrome_process_finder_win.cc',
        'browser/chrome_process_finder_win.h',
      ],
    },
  ],
}
