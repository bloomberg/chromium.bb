# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'zip',
      'type': 'static_library',
      'dependencies': [
        '../third_party/zlib/zlib.gyp:minizip',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'zip/zip.cc',
        'zip/zip.h',
        'zip/zip_internal.cc',
        'zip/zip_internal.h',
        'zip/zip_reader.cc',
        'zip/zip_reader.h',
      ],
    },
  ],
}
