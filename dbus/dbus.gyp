# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'dbus',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../build/linux/system.gyp:dbus',
      ],
      'sources': [
      ],
    },
  ],
}
