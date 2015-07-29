# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'security_interstitials',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'history_core_browser',
        'metrics',
        'rappor',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'security_interstitials/metrics_helper.cc',
        'security_interstitials/metrics_helper.h',
      ]
    }
  ]
}