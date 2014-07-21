# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Cross-platform user_manager sources.
    'user_manager_shared_sources': [
      'user_manager/empty_user_info.cc',
      'user_manager/empty_user_info.h',
      'user_manager/user_info.h',
      'user_manager/user_info.cc',
      'user_manager/user_info_impl.cc',
      'user_manager/user_info_impl.h',
      'user_manager/user_manager_export.h',
    ],
    # Chrome OS user_manager sources.
    'user_manager_chromeos_sources': [
      'user_manager/user_image/default_user_images.cc',
      'user_manager/user_image/default_user_images.h',
      'user_manager/user_image/user_image.cc',
      'user_manager/user_image/user_image.h',
      'user_manager/user_type.h',
    ],
  },
  'targets': [{
    'target_name': 'user_manager',
    'type': '<(component)',
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'defines': [
      'USER_MANAGER_IMPLEMENTATION',
    ],
    'include_dirs': [
      '<(DEPTH)',
      '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
      '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
    ],
    'sources': [ '<@(user_manager_shared_sources)' ],
    'conditions': [
      ['chromeos == 1', {
        'dependencies': [
          '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
          '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
        ],
        'sources': [ '<@(user_manager_chromeos_sources)' ],
      }],
    ],
  }],
}
