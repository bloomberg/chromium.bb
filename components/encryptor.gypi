# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'encryptor',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        'encryptor/encryptor_switches.cc',
        'encryptor/encryptor_switches.h',
        'encryptor/ie7_password_win.cc',
        'encryptor/ie7_password_win.h',
        'encryptor/keychain_password_mac.h',
        'encryptor/keychain_password_mac.mm',
        'encryptor/os_crypt.h',
        'encryptor/os_crypt_mac.mm',
        'encryptor/os_crypt_posix.cc',
        'encryptor/os_crypt_win.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'encryptor/os_crypt_posix.cc',
          ],
        }],
      ],
      'target_conditions': [
        ['OS=="ios"', {
          'sources/': [
            ['include', '^encryptor/keychain_password_mac\\.mm$'],
            ['include', '^encryptor/os_crypt_mac\\.mm$'],
          ],
        }],
      ],
    },
  ],
}
