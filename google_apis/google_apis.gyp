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
      'conditions': [
        ['google_api_key!=""', {
          'defines': ['GOOGLE_API_KEY="<(google_api_key)"'],
        }],
        # Once the default definitions for the various keys in
        # google_apis/google_api_keys.cc are all made empty, the next
        # two conditionals can set just GOOGLE_DEFAULT_CLIENT_ID/SECRET.
        # Until then, we have different semantics on the gyp variables
        # google_default_client_id/secret and setting the environment
        # variables of the (upper-case) same name (the latter are used
        # as the default for unset client IDs/secrets, whereas the
        # former overrides all client IDs/secrets).
        # TODO(joi): Fix the above semantic mismatch once possible.
        ['google_default_client_id!=""', {
          'defines': [
            'GOOGLE_CLIENT_ID_MAIN="<(google_default_client_id)"',
            'GOOGLE_CLIENT_ID_CLOUD_PRINT="<(google_default_client_id)"',
            'GOOGLE_CLIENT_ID_REMOTING="<(google_default_client_id)"',
          ]
        }],
        ['google_default_client_secret!=""', {
          'defines': [
            'GOOGLE_CLIENT_SECRET_MAIN="<(google_default_client_secret)"',
            'GOOGLE_CLIENT_SECRET_CLOUD_PRINT="<(google_default_client_secret)"',
            'GOOGLE_CLIENT_SECRET_REMOTING="<(google_default_client_secret)"',
          ]
        }],
        ['use_official_google_api_keys==1', {
          'defines': ['USE_OFFICIAL_GOOGLE_API_KEYS=1'],
        }],
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
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
