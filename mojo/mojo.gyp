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
    },
  ],
}
