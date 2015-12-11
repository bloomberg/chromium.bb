# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is intentionally a gyp file rather than a gypi for dependencies
# reasons. The other gypi files include content.gyp and content_common depends
# on this, thus if you try to rename this to gypi and include it in
# components.gyp, you will get a circular dependency error.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets' : [
    {
      'target_name': 'tracing',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'TRACING_IMPLEMENTATION=1',
      ],
      'sources': [
        'tracing/child_memory_dump_manager_delegate_impl.cc',
        'tracing/child_memory_dump_manager_delegate_impl.h',
        'tracing/child_trace_message_filter.cc',
        'tracing/child_trace_message_filter.h',
        'tracing/graphics_memory_dump_provider_android.cc',
        'tracing/graphics_memory_dump_provider_android.h',
        'tracing/trace_config_file.cc',
        'tracing/trace_config_file.h',
        'tracing/trace_to_console.cc',
        'tracing/trace_to_console.h',
        'tracing/tracing_export.h',
        'tracing/tracing_messages.cc',
        'tracing/tracing_messages.h',
        'tracing/tracing_switches.cc',
        'tracing/tracing_switches.h',
      ],
    },
  ],
}
