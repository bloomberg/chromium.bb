# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'user_manager',
    'type': '<(component)',
    'dependencies': [
      '../base/base.gyp:base',
    ],
    'defines': [
      'USER_MANAGER_IMPLEMENTATION',
    ],
    'include_dirs': [
      '..',
    ],
    'sources': [
      'user_manager/user_type.h',
      'user_manager/user_manager_export.h',
    ],
  }],
}
