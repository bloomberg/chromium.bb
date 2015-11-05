# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/browser_sync_common
      'target_name': 'browser_sync_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/common/browser_sync_switches.cc',
        'browser_sync/common/browser_sync_switches.h',
      ],
    },
  ],
}
