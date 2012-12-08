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
        'components/tracing/child_trace_message_filter.cc',
        'components/tracing/child_trace_message_filter.h',
        'components/tracing/tracing_messages.cc',
        'components/tracing/tracing_messages.h',
      ],
    },
  ],
}
