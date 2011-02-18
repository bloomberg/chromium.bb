# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_common',
      'type': '<(library)',
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': '062E9260-304A-4657-A74C-0D3AA1A0A0A4',
        }],
      ],
    },
  ],
}
