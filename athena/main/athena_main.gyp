# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_main',
      'type': 'executable',
      'dependencies': [
        '../../apps/shell/app_shell.gyp:app_shell_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_main.cc',
      ],
    },
  ],  # targets
}
