# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'history_core_browser',
      'type': 'none',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'keyed_service_core',
      ],
      'sources': [
        'history/core/browser/history_client.h',
      ],
    },
  ],
}
