# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'includes': [
    'content_tests.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        '../build/win_precompile.gypi',
        'content_shell.gypi',
      ],
    }],
  ],
}
