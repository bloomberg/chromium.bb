# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'user_manager',
    'type': '<(component)',
    'dependencies': [
      '../base/base.gyp:base',
      '../skia/skia.gyp:skia',
      '../ui/gfx/gfx.gyp:gfx',
      '../url/url.gyp:url_lib',
    ],
    'defines': [
      'USER_MANAGER_IMPLEMENTATION',
    ],
    'include_dirs': [
      '..',
    ],
    'sources': [
      'user_manager/user_image/user_image.cc',
      'user_manager/user_image/user_image.h',
      'user_manager/user_type.h',
      'user_manager/user_manager_export.h',
    ],
  }],
}
