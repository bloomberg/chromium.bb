# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    ['OS=="linux"', {
      'targets' : [
        {
          'target_name': 'omx_test',
          'type': 'executable',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../third_party/openmax/openmax.gyp:il',
          ],
          'sources': [
            'input_buffer.cc',
            'input_buffer.h',
            'omx_test.cc',
            'omx_video_decoder.cc',
            'omx_video_decoder.h',
          ],
        },
      ],
    }],
  ],
}
