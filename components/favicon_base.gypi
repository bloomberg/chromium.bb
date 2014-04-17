# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'favicon_base',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'favicon_base/favicon_types.cc',
        'favicon_base/favicon_types.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
