# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'data_usage_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'data_usage/core/data_use.cc',
        'data_usage/core/data_use.h',
        'data_usage/core/data_use_aggregator.cc',
        'data_usage/core/data_use_aggregator.h',
        'data_usage/core/data_use_annotator.h',
      ]
    },
  ]
}
