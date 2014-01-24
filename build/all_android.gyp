# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(thakis): Remove this file after https://codereview.chromium.org/139743016
# has been in the tree for a while.
{
  'targets': [
    {
      'target_name': 'temporary_android_dummy_target',
      'dependencies': [
        'all.gyp:*',
      ],
      'type': 'none',
    },
  ],
}
