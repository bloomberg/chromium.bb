# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/previews
      'target_name': 'previews',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'variations',
      ],
      'sources': [
        'previews/previews_experiments.cc',
        'previews/previews_experiments.h',
      ]
    },
  ],
}
