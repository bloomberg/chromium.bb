# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'app_framework',
      'type': 'static_library',
      'dependencies': [
        '../gpu.gyp:command_buffer_client',
        '../gpu.gyp:command_buffer_service',
      ],
      'sources': [
        'app_framework/application.cc',
        'app_framework/application.h',
        'app_framework/platform.h',
      ],
    },
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
