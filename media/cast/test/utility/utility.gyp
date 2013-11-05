# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_test_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'sources': [
        'input_helper.cc',
        'input_helper.',
      ], # source
    },
  ],
}