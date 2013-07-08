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
      'includes': [
        'determine_use_official_keys.gypi',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
      ],
      'conditions': [
        ['google_api_key!=""', {
          'defines': ['GOOGLE_API_KEY="<(google_api_key)"'],
        }],
        ['google_default_client_id!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_ID="<(google_default_client_id)"',
          ]
        }],
        ['google_default_client_secret!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_SECRET="<(google_default_client_secret)"',
          ]
        }],
        [ 'OS == "android"', {
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
            ],
            'sources/': [
              ['exclude', 'cup/client_update_protocol_nss\.cc$'],
            ],
        }],
        [ 'use_openssl==1', {
            'sources!': [
              'cup/client_update_protocol_nss.cc',
            ],
          }, {
            'sources!': [
              'cup/client_update_protocol_openssl.cc',
            ],
        },],
      ],
      'sources': [
        'cup/client_update_protocol.cc',
        'cup/client_update_protocol.h',
        'cup/client_update_protocol_nss.cc',
        'cup/client_update_protocol_openssl.cc',
        'gaia/gaia_auth_consumer.cc',
        'gaia/gaia_auth_consumer.h',
        'gaia/gaia_auth_fetcher.cc',
        'gaia/gaia_auth_fetcher.h',
        'gaia/gaia_auth_util.cc',
        'gaia/gaia_auth_util.h',
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
        'gaia/oauth2_mint_token_flow.cc',
        'gaia/oauth2_mint_token_flow.h',
        'google_api_keys.cc',
        'google_api_keys.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
