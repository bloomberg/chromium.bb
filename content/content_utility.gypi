# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_utility',
      'type': 'static_library',
      'dependencies': [
        'content_common',
        '../base/base.gyp:base',
      ],
      'sources': [
        'utility/content_utility_client.h',
        'utility/utility_main.cc',
        'utility/utility_thread.cc',
        'utility/utility_thread.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'mac_bundle_resources': [
              'utility/utility.sb',
            ],
          },
        }],
      ],
    },
  ],
}
