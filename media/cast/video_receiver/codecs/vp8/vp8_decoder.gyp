# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of the source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_vp8_decoder',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/libvpx/',
      ],
      'sources': [
        'vp8_decoder.cc',
        'vp8_decoder.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
      ],
    },
  ],
}
