# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'google_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'google/core/browser/google_search_metrics.cc',
        'google/core/browser/google_search_metrics.h',
        'google/core/browser/google_url_tracker_client.cc',
        'google/core/browser/google_url_tracker_client.h',
      ],
    },
  ],
}
