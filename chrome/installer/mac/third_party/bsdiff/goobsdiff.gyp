# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'type': 'executable',
    'link_settings': {
      'libraries': [
        '$(SDKROOT)/usr/lib/libbz2.dylib',
        '$(SDKROOT)/usr/lib/libz.dylib',
        '$(SDKROOT)/usr/lib/libcrypto.dylib',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'goobsdiff',
      'dependencies': [
        '../xz/xz.gyp:lzma',
      ],
      'sources': [
        'goobsdiff.c',
      ],
    },
    {
      'target_name': 'goobspatch',
      'dependencies': [
        '../xz/xz.gyp:lzma_decompress',
      ],
      'sources': [
        'goobspatch.c',
      ],
      'configurations': {
        'Release': {
          'xcode_settings': {
            # Use -Os to minimize the size of the installer tools.
            'GCC_OPTIMIZATION_LEVEL': 's',
          },
        },
      },
    },
  ],
}
