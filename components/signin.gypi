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
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        'keyed_service_core',
        'os_crypt',
        'webdata_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'signin/core/mutable_profile_oauth2_token_service.cc',
        'signin/core/mutable_profile_oauth2_token_service.h',
        'signin/core/profile_oauth2_token_service.cc',
        'signin/core/profile_oauth2_token_service.h',
        'signin/core/signin_client.h',
        'signin/core/signin_error_controller.cc',
        'signin/core/signin_error_controller.h',
        'signin/core/signin_manager_cookie_helper.cc',
        'signin/core/signin_manager_cookie_helper.h',
        'signin/core/webdata/token_service_table.cc',
        'signin/core/webdata/token_service_table.h',
        'signin/core/webdata/token_web_data.cc',
        'signin/core/webdata/token_web_data.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'sources!': [
        # Not used on Android.
        'signin/core/mutable_profile_oauth2_token_service.cc',
        'signin/core/mutable_profile_oauth2_token_service.h',
      ],
    }],
  ],
}
