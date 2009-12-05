# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'breakpad_sender_target': 0,
    },
    'target_conditions': [
      ['breakpad_sender_target==1', {
        'sources': [
          'src/client/windows/sender/crash_report_sender.cc',
          'src/common/windows/http_upload.cc',
          'src/client/windows/sender/crash_report_sender.h',
          'src/common/windows/http_upload.h',
        ],
        'include_dirs': [
          'src',
        ],
        'link_settings': {
          'libraries': [
            '-lurlmon.lib',
          ],
        },
      }],
    ],
  },
  'conditions': [
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_sender',
          'type': '<(library)',
          'msvs_guid': '9946A048-043B-4F8F-9E07-9297B204714C',
          'variables': {
            'breakpad_sender_target': 1,
          },
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 64-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
        },
        {
          'target_name': 'breakpad_sender_win64',
          'type': '<(library)',
          'msvs_guid': '237AEB58-9D74-41EF-9D49-A6ECE24EA8BC',
          'variables': {
            'breakpad_sender_target': 1,
          },
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
          'configurations': {
            'Common': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
