# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_vp8_encoder',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
         '<(DEPTH)/third_party/',
      ],
      'sources': [
        'vp8_encoder.cc',
        'vp8_encoder.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
      ],
    },
  ],
}
