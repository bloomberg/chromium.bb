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
        'encryptor/encryptor.h',
        'encryptor/encryptor_mac.mm',
        'encryptor/encryptor_password_mac.h',
        'encryptor/encryptor_password_mac.mm',
        'encryptor/encryptor_posix.cc',
        'encryptor/encryptor_switches.cc',
        'encryptor/encryptor_switches.h',
        'encryptor/encryptor_win.cc',
        'encryptor/ie7_password_win.cc',
        'encryptor/ie7_password_win.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'encryptor/encryptor_posix.cc',
          ],
        }],
      ],
      'target_conditions': [
        ['OS=="ios"', {
          'sources/': [
            ['include', '^encryptor/encryptor_mac\\.mm$'],
            ['include', '^encryptor/encryptor_password_mac\\.mm$'],
          ],
        }],
      ],
    },
  ],
}
