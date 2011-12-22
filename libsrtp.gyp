# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      'HAVE_STDLIB_H',
      'HAVE_STRING_H',
    ],
    'include_dirs': [
      './config',
      'srtp/include',
      'srtp/crypto/include',
    ],
    'conditions': [
      ['target_arch=="x64"', {
        'defines': [
          'CPU_CISC',
          'SIZEOF_UNSIGNED_LONG=8',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['target_arch=="ia32"', {
        'defines': [
          'CPU_CISC',
          'SIZEOF_UNSIGNED_LONG=4',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'CPU_RISC',
          'SIZEOF_UNSIGNED_LONG=4',
          'SIZEOF_UNSIGNED_LONG_LONG=8',
        ],
      }],
      ['OS!="win"', {
        'defines': [
          'HAVE_STDINT_H',
          'HAVE_INTTYPES_H',
         ],
      }],
      ['OS=="win"', {
        'defines': [
          'HAVE_WINSOCK2_H',
          'inline=__inline',
         ],
      }],
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './config',
        'srtp/include',
        'srtp/crypto/include',
      ],
      'conditions': [
        ['target_arch=="x64"', {
          'defines': [
            'CPU_CISC',
            'SIZEOF_UNSIGNED_LONG=8',
            'SIZEOF_UNSIGNED_LONG_LONG=8',
          ],
        }],
        ['target_arch=="ia32"', {
          'defines': [
            'CPU_CISC',
            'SIZEOF_UNSIGNED_LONG=4',
            'SIZEOF_UNSIGNED_LONG_LONG=8',
          ],
        }],
        ['target_arch=="arm"', {
          'defines': [
            'CPU_RISC',
            'SIZEOF_UNSIGNED_LONG=4',
            'SIZEOF_UNSIGNED_LONG_LONG=8',
          ],
        }],
        ['OS!="win"', {
          'defines': [
            'HAVE_STDINT_H',
            'HAVE_INTTYPES_H',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'HAVE_WINSOCK2_H',
            'inline=__inline',
          ],
        }],
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libsrtp',
      'type': 'static_library',
      'sources': [
        # includes
        'srtp/config_in.h',
        'srtp/include/ekt.h',
        'srtp/include/getopt_s.h',
        'srtp/include/rtp.h',
        'srtp/include/rtp_priv.h',
        'srtp/include/srtp.h',
        'srtp/include/srtp_priv.h',
        'srtp/include/ut_sim.h',

        # headers
        'srtp/crypto/aes_cbc.h',
        'srtp/crypto/aes.h',
        'srtp/crypto/aes_icm.h',
        'srtp/crypto/alloc.h',
        'srtp/crypto/auth.h',
        'srtp/crypto/cipher.h',
        'srtp/crypto/config.h',
        'srtp/crypto/cryptoalg.h',
        'srtp/crypto/crypto.h',
        'srtp/crypto/crypto_kernel.h',
        'srtp/crypto/crypto_math.h',
        'srtp/crypto/crypto_types.h',
        'srtp/crypto/datatypes.h',
        'srtp/crypto/err.h',
        'srtp/crypto/gf2_8.h',
        'srtp/crypto/hmac.h',
        'srtp/crypto/integers.h',
        'srtp/crypto/kernel_compat.h',
        'srtp/crypto/key.h',
        'srtp/crypto/null_auth.h',
        'srtp/crypto/null_cipher.h',
        'srtp/crypto/prng.h',
        'srtp/crypto/rand_source.h',
        'srtp/crypto/rdb.h',
        'srtp/crypto/rdbx.h',
        'srtp/crypto/sha1.h',
        'srtp/crypto/stat.h',
        'srtp/crypto/xfm.h',

        # sources
        'srtp/srtp/ekt.c',
        'srtp/srtp/srtp.c',
        
        'srtp/crypto/cipher/aes.c',
        'srtp/crypto/cipher/aes_cbc.c',
        'srtp/crypto/cipher/aes_icm.c',
        'srtp/crypto/cipher/cipher.c',
        'srtp/crypto/cipher/null_cipher.c',
        'srtp/crypto/hash/auth.c',
        'srtp/crypto/hash/hmac.c',
        'srtp/crypto/hash/null_auth.c',
        'srtp/crypto/hash/sha1.c',
        'srtp/crypto/kernel/alloc.c',
        'srtp/crypto/kernel/crypto_kernel.c',
        'srtp/crypto/kernel/err.c',
        'srtp/crypto/kernel/key.c',
        'srtp/crypto/math/datatypes.c',
        'srtp/crypto/math/gf2_8.c',
        'srtp/crypto/math/math.c',
        'srtp/crypto/math/stat.c',
        'srtp/crypto/replay/rdb.c',
        'srtp/crypto/replay/rdbx.c',
        'srtp/crypto/replay/ut_sim.c',
        'srtp/crypto/rng/ctr_prng.c',
        'srtp/crypto/rng/prng.c',
        'srtp/crypto/rng/rand_source.c',
      ],
      'conditions': [
        ['OS=="linux"', {
         'sources': [
           'srtp/crypto/rng/rand_linux_kernel.c',
         ],
        }],
      ]
    },
  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
