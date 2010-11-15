# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'call_broker',
      'type': 'executable',
      'sources': [
        'call_broker.cc',
        'precompile.cc',
        'precompile.h',
      ],
      'defines': [
        '_CONSOLE',
      ],
      'dependencies': [
        '<(DEPTH)/ceee/ie/plugin/toolband/toolband.gyp:toolband_idl',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_common',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
      ],
    },
  ]
}
