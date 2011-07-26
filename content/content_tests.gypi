# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
