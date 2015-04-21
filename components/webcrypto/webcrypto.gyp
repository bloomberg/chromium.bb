# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webcrypto',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../tracing.gyp:tracing',
        '../../skia/skia.gyp:skia',
        '../../ui/base/ui_base.gyp:ui_base',
        '../../ui/events/events.gyp:gestures_blink',
        '../../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
      'variables': {
        'webcrypto_sources': [
          'algorithm_dispatch.cc',
          'algorithm_dispatch.h',
          'algorithm_implementation.cc',
          'algorithm_implementation.h',
          'algorithm_registry.cc',
          'algorithm_registry.h',
          'crypto_data.cc',
          'crypto_data.h',
          'generate_key_result.cc',
          'generate_key_result.h',
          'jwk.cc',
          'jwk.h',
          'platform_crypto.h',
          'status.cc',
          'status.h',
          'webcrypto_impl.cc',
          'webcrypto_impl.h',
          'webcrypto_util.cc',
          'webcrypto_util.h',
        ],
        'webcrypto_nss_sources': [
          'nss/aes_algorithm_nss.cc',
          'nss/aes_algorithm_nss.h',
          'nss/aes_cbc_nss.cc',
          'nss/aes_gcm_nss.cc',
          'nss/aes_kw_nss.cc',
          'nss/hmac_nss.cc',
          'nss/key_nss.cc',
          'nss/key_nss.h',
          'nss/rsa_hashed_algorithm_nss.cc',
          'nss/rsa_hashed_algorithm_nss.h',
          'nss/rsa_oaep_nss.cc',
          'nss/rsa_ssa_nss.cc',
          'nss/sha_nss.cc',
          'nss/sym_key_nss.cc',
          'nss/sym_key_nss.h',
          'nss/util_nss.cc',
          'nss/util_nss.h',
        ],
        'webcrypto_openssl_sources': [
          'openssl/aes_algorithm_openssl.cc',
          'openssl/aes_algorithm_openssl.h',
          'openssl/aes_cbc_openssl.cc',
          'openssl/aes_ctr_openssl.cc',
          'openssl/aes_gcm_openssl.cc',
          'openssl/aes_kw_openssl.cc',
          'openssl/ec_algorithm_openssl.cc',
          'openssl/ec_algorithm_openssl.h',
          'openssl/ecdh_openssl.cc',
          'openssl/ecdsa_openssl.cc',
          'openssl/hkdf_openssl.cc',
          'openssl/hmac_openssl.cc',
          'openssl/key_openssl.cc',
          'openssl/key_openssl.h',
          'openssl/pbkdf2_openssl.cc',
          'openssl/rsa_hashed_algorithm_openssl.cc',
          'openssl/rsa_hashed_algorithm_openssl.h',
          'openssl/rsa_oaep_openssl.cc',
          'openssl/rsa_pss_openssl.cc',
          'openssl/rsa_sign_openssl.cc',
          'openssl/rsa_sign_openssl.h',
          'openssl/rsa_ssa_openssl.cc',
          'openssl/sha_openssl.cc',
          'openssl/util_openssl.cc',
          'openssl/util_openssl.h',
        ],
      },
      'sources': [
        '<@(webcrypto_sources)',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '../../build/android/ndk.gyp:cpu_features',
          ],
        }],
        ['use_openssl==1', {
          'sources': [
            '<@(webcrypto_openssl_sources)',
          ],
          'dependencies': [
            '../../third_party/boringssl/boringssl.gyp:boringssl',
          ],
        }, {
          'sources': [
            '<@(webcrypto_nss_sources)',
          ],
          'conditions': [
            ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
              'dependencies': [
                '../../build/linux/system.gyp:ssl',
              ],
            }, {
              'dependencies': [
                '../../third_party/nss/nss.gyp:nspr',
                '../../third_party/nss/nss.gyp:nss',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
