# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'crx_file',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'CRX_FILE_IMPLEMENTATION',
      ],
      'sources': [
        'crx_file/constants.h',
        'crx_file/crx_file.cc',
        'crx_file/crx_file.h',
      ],
    },
  ],
}
