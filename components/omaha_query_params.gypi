# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'omaha_query_params',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'omaha_query_params/omaha_query_params.cc',
        'omaha_query_params/omaha_query_params.h',
        'omaha_query_params/omaha_query_params_delegate.cc',
        'omaha_query_params/omaha_query_params_delegate.h',
      ],
    },
  ],
}
