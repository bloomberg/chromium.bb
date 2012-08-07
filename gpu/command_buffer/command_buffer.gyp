# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gles2_utils',
      'type': '<(component)',
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos',
        ],
      },
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
      'defines': [
        'GLES2_UTILS_IMPLEMENTATION',
      ],
      'sources': [
        'common/gles2_cmd_format.h',
        'common/gles2_cmd_utils.cc',
        'common/gles2_cmd_utils.h',
        'common/gles2_utils_export.h',
        'common/logging.cc',
        'common/logging.h',
      ],
    },
  ],
}

