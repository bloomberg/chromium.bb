# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'transport_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
      ],
      'sources': [
        'transport_encryption_handler.cc',
        'transport_encryption_handler.h',
      ],
    },
  ],
}