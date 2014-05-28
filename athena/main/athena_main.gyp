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
        '../athena.gyp:athena_lib',
        '../../apps/shell/app_shell.gyp:app_shell_lib',
        '../../skia/skia.gyp:skia',
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../../ui/views/views.gyp:views',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_launcher.cc',
        'athena_launcher.h',
        'athena_main.cc',
        'placeholder.cc',
        'placeholder.h',
      ],
    },
  ],  # targets
}
