# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'conditions': [
        ['inside_chromium_build==1', {
          'webkit_src_dir': '<(DEPTH)/third_party/WebKit',
        }, {
          'webkit_src_dir': '<(DEPTH)/../../..',
        }],
      ],
    },
    'webkit_src_dir': '<(webkit_src_dir)',
    'conditions': [
      ['inside_chromium_build==1', {
        'cc_stubs_dirs': ['stubs'],
      }, {
        'cc_stubs_dirs': [
          'stubs',
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(webkit_src_dir)',
          '<(webkit_src_dir)/Source/WebCore/platform',
          '<(webkit_src_dir)/Source/WebCore/platform/animation',
          '<(webkit_src_dir)/Source/WebCore/platform/chromium',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics/chromium',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics/filters',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics/gpu',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics/skia',
          '<(webkit_src_dir)/Source/WebCore/platform/graphics/transforms',
          '<(webkit_src_dir)/Source/WebCore/rendering',
          '<(webkit_src_dir)/Source/WebCore/rendering/style',
        ],
      }],
    ],
  },
  'conditions': [
    ['inside_chromium_build==0', {
      'defines': [
        'INSIDE_WEBKIT_BUILD=1',
      ],
    }],
  ],
}
