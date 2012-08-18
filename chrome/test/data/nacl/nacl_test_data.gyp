# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'nacl_browser_test.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_tests',
      'type': 'none',
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'build_glibc': 1,
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'nacl_load_test.html',
        ],
      },
    },
  ],
}