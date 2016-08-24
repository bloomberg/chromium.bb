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
        'tracing/browser/trace_config_file.cc',
        'tracing/browser/trace_config_file.h',
        'tracing/child/child_memory_dump_manager_delegate_impl.cc',
        'tracing/child/child_memory_dump_manager_delegate_impl.h',
        'tracing/child/child_trace_message_filter.cc',
        'tracing/child/child_trace_message_filter.h',
        'tracing/common/graphics_memory_dump_provider_android.cc',
        'tracing/common/graphics_memory_dump_provider_android.h',
        'tracing/common/process_metrics_memory_dump_provider.cc',
        'tracing/common/process_metrics_memory_dump_provider.h',
        'tracing/common/trace_to_console.cc',
        'tracing/common/trace_to_console.h',
        'tracing/common/tracing_messages.cc',
        'tracing/common/tracing_messages.h',
        'tracing/common/tracing_switches.cc',
        'tracing/common/tracing_switches.h',
        'tracing/core/proto_utils.h',
        'tracing/core/proto_zero_message.cc',
        'tracing/core/proto_zero_message.h',
        'tracing/core/proto_zero_message_handle.cc',
        'tracing/core/proto_zero_message_handle.h',
        'tracing/core/scattered_stream_writer.cc',
        'tracing/core/scattered_stream_writer.h',
        'tracing/core/trace_buffer_writer.cc',
        'tracing/core/trace_buffer_writer.h',
        'tracing/core/trace_ring_buffer.cc',
        'tracing/core/trace_ring_buffer.h',
        'tracing/tracing_export.h',
      ],
      'target_conditions': [
        ['>(nacl_untrusted_build)==1', {
          'sources!': [
            'tracing/common/process_metrics_memory_dump_provider.cc',
          ],
        }],
      ]
    },
    {
      'target_name': 'proto_zero_plugin',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'tracing/tools/proto_zero_plugin/proto_zero_generator.cc',
        'tracing/tools/proto_zero_plugin/proto_zero_generator.h',
        'tracing/tools/proto_zero_plugin/proto_zero_plugin.cc',
      ],
      'include_dirs': [
        '..',
        '../third_party/protobuf/src',
      ],
      'dependencies': [
        '../third_party/protobuf/protobuf.gyp:protoc_lib',
      ],
    },
    {
      'target_name': 'proto_zero_testing_messages',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'tracing/test',
        'proto_out_dir': 'components/tracing/test',
        'generator_plugin': 'proto_zero_plugin',
        'generator_plugin_suffix': '.pbzero',
        'generate_cc': 0,
        'generate_python': 0,
      },
      'sources': [
        'tracing/test/example_messages.proto',
      ],
      'dependencies': [
        'proto_zero_plugin#host',
      ],
      'includes': ['../build/protoc.gypi'],
    },
    {
      # Official protobuf used by tests to verify that the Tracing V2 output is
      # effectively proto-compatible.
      # GN version: //components/tracing:golden_protos_for_tests
      'target_name': 'golden_protos_for_tests',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'tracing/proto',
        'proto_out_dir': 'components/tracing/test/golden_protos',
      },
      'sources': [
        'tracing/proto/events_chunk.proto',
      ],
      'includes': ['../build/protoc.gypi'],
    },
  ],
}
