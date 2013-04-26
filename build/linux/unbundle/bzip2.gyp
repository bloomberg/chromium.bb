# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'bzip2',
      'type': 'none',
      'direct_dependent_settings': {
        'defines': [
          'USE_SYSTEM_LIBBZ2',
        ],
      },
      'link_settings': {
        'libraries': [
          '-lbz2',
        ],
      },
    },
  ],
}
