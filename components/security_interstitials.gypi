# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/security_interstitials/core
      'target_name': 'security_interstitials_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        'components_strings.gyp:components_strings',
        'google_core_browser',
        'history_core_browser',
        'metrics',
        'rappor',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'security_interstitials/core/controller_client.cc',
        'security_interstitials/core/controller_client.h',
        'security_interstitials/core/metrics_helper.cc',
        'security_interstitials/core/metrics_helper.h',
      ]
    }
  ]
}