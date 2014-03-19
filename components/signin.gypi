# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'signin_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../sql/sql.gyp:sql',
        'os_crypt',
        'webdata_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/signin_client.h',
        'signin/core/signin_manager_cookie_helper.cc',
        'signin/core/signin_manager_cookie_helper.h',
        'signin/core/webdata/token_service_table.cc',
        'signin/core/webdata/token_service_table.h',
        'signin/core/webdata/token_web_data.cc',
        'signin/core/webdata/token_web_data.h',
      ],
    },
  ],
}
