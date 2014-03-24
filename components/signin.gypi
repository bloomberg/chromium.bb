# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'signin_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
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
        'signin/core/browser/mutable_profile_oauth2_token_service.cc',
        'signin/core/browser/mutable_profile_oauth2_token_service.h',
        'signin/core/browser/profile_oauth2_token_service.cc',
        'signin/core/browser/profile_oauth2_token_service.h',
        'signin/core/browser/signin_client.h',
        'signin/core/browser/signin_error_controller.cc',
        'signin/core/browser/signin_error_controller.h',
        'signin/core/browser/signin_internals_util.cc',
        'signin/core/browser/signin_internals_util.h',
        'signin/core/browser/signin_manager_cookie_helper.cc',
        'signin/core/browser/signin_manager_cookie_helper.h',
        'signin/core/browser/webdata/token_service_table.cc',
        'signin/core/browser/webdata/token_service_table.h',
        'signin/core/browser/webdata/token_web_data.cc',
        'signin/core/browser/webdata/token_web_data.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'sources!': [
        # Not used on Android.
        'signin/core/browser/mutable_profile_oauth2_token_service.cc',
        'signin/core/browser/mutable_profile_oauth2_token_service.h',
      ],
    }],
  ],
}
