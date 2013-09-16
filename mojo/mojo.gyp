# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'target_defaults': {
    'defines': ['MOJO_IMPLEMENTATION'],
  },
  'targets': [
    {
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'mojo_message',
      ],
    },
    {
      'target_name': 'mojo_message',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'public/libs/message/message.cc',
        'public/libs/message/message.h',
        'public/libs/message/message_builder.cc',
        'public/libs/message/message_builder.h',
      ],
    },
    {
      'target_name': 'mojo_unittests',
      'type': 'executable',
      'dependencies': [
        'mojo_message',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/libs/message/message_unittest.cc',
      ],
    },
  ],
}
