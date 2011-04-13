# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'candidate_window',
      'type': 'executable',
      'sources': [
        'candidate_window_main.cc',
      ],
    },
  ],
}
