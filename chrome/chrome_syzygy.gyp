# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win" and fastbuild==0', {
      'variables': {
        'dll_name': 'chrome',
      },
      'targets': [
        {
          'target_name': 'chrome_dll_syzygy',
          'type': 'none',
          'sources' : [],
          'includes': [
            'chrome_syzygy.gypi',
          ],
        },
      ],
    }],
    # Note, not else.
    ['OS=="win" and fastbuild==0 and chrome_multiple_dll==1', {
      'variables': {
        'dll_name': 'chrome_child',
      },
      'targets': [
        {
          'target_name': 'chrome_child_dll_syzygy',
          'type': 'none',
          'sources' : [],
          'includes': [
            'chrome_syzygy.gypi',
          ],
        },
      ],
    }],
  ],
}
