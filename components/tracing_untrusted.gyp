# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'tracing_untrusted',
          'type': 'none',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            '../base/base_untrusted.gyp:base_untrusted',
            '../native_client/tools.gyp:prep_toolchain',
            '../ipc/ipc.gyp:ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libtracing_untrusted.a',
            'build_glibc': 0,
            'build_newlib': 1,
          },
          'sources': [
            'tracing/child_trace_message_filter.cc',
            'tracing/child_trace_message_filter.h',
            'tracing/tracing_messages.cc',
            'tracing/tracing_messages.h',
          ],
        },
      ],
    }],
  ],
}
