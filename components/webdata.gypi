# Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
        'webdata/encryptor/encryptor.h',
        'webdata/encryptor/encryptor_mac.mm',
        'webdata/encryptor/encryptor_password_mac.h',
        'webdata/encryptor/encryptor_password_mac.mm',
        'webdata/encryptor/encryptor_posix.cc',
        'webdata/encryptor/encryptor_win.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'webdata/encryptor/encryptor_posix.cc',
          ],
        }],
      ],
    },
  ],
}
