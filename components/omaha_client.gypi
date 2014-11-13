# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'omaha_client',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'omaha_client/omaha_query_params.cc',
        'omaha_client/omaha_query_params.h',
        'omaha_client/omaha_query_params_delegate.cc',
        'omaha_client/omaha_query_params_delegate.h',
      ],
    },
  ],
}
