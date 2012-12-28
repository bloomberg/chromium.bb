# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets' : [
    {
      'target_name': 'tracing',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'tracing/child_trace_message_filter.cc',
        'tracing/child_trace_message_filter.h',
        'tracing/tracing_messages.cc',
        'tracing/tracing_messages.h',
      ],
    },
  ],
}
