# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'proximity_auth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        "proximity_auth/proximity_auth_system.cc",
        "proximity_auth/proximity_auth_system.h",
        "proximity_auth/remote_device.h",
      ],
    },
  ],
}
