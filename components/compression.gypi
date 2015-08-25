# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/compression
      'target_name': 'compression',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        'compression/compression_utils.cc',
        'compression/compression_utils.h',
      ],
    },
  ],
}
