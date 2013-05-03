# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ffmpeg',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags libavcodec libavformat libavutil)',
        ],
        'defines': [
          '__STDC_CONSTANT_MACROS',
          'USE_SYSTEM_FFMPEG',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other libavcodec libavformat libavutil)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l libavcodec libavformat libavutil)',
        ],
      },
    },
  ],
}
