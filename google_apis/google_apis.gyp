# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'google_apis',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
      ],
      'sources': [
        'gaia/gaia_auth_consumer.cc',
        'gaia/gaia_auth_consumer.h',
        'gaia/gaia_auth_fetcher.cc',
        'gaia/gaia_auth_fetcher.h',
        'gaia/gaia_auth_util.cc',
        'gaia/gaia_auth_util.h',
        'gaia/gaia_authenticator.cc',
        'gaia/gaia_authenticator.h',
        'gaia/gaia_constants.cc',
        'gaia/gaia_constants.h',
        'gaia/gaia_oauth_client.cc',
        'gaia/gaia_oauth_client.h',
        'gaia/gaia_switches.cc',
        'gaia/gaia_switches.h',
        'gaia/gaia_urls.cc',
        'gaia/gaia_urls.h',
        'gaia/google_service_auth_error.cc',
        'gaia/google_service_auth_error.h',
        'gaia/oauth_request_signer.cc',
        'gaia/oauth_request_signer.h',
        'gaia/oauth2_access_token_consumer.h',
        'gaia/oauth2_access_token_fetcher.cc',
        'gaia/oauth2_access_token_fetcher.h',
        'gaia/oauth2_api_call_flow.cc',
        'gaia/oauth2_api_call_flow.h',
        'gaia/oauth2_mint_token_consumer.h',
        'gaia/oauth2_mint_token_fetcher.cc',
        'gaia/oauth2_mint_token_fetcher.h',
        'gaia/oauth2_mint_token_flow.cc',
        'gaia/oauth2_mint_token_flow.h',
        'gaia/oauth2_revocation_consumer.h',
        'gaia/oauth2_revocation_fetcher.cc',
        'gaia/oauth2_revocation_fetcher.h',
        'google_api_keys.cc',
        'google_api_keys.h',
      ],
    },
    {
      'target_name': 'google_apis_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'gaia/mock_url_fetcher_factory.h',
        'gaia/gaia_auth_fetcher_unittest.cc',
        'gaia/gaia_auth_util_unittest.cc',
        'gaia/gaia_authenticator_unittest.cc',
        'gaia/gaia_oauth_client_unittest.cc',
        'gaia/google_service_auth_error_unittest.cc',
        'gaia/oauth_request_signer_unittest.cc',
        'gaia/oauth2_access_token_fetcher_unittest.cc',
        'gaia/oauth2_api_call_flow_unittest.cc',
        'gaia/oauth2_mint_token_fetcher_unittest.cc',
        'gaia/oauth2_mint_token_flow_unittest.cc',
        'gaia/oauth2_revocation_fetcher_unittest.cc',
      ],
    },
  ],
}
