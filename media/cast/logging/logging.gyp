# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cast_common_logging',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_logging_proto_lib',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        'cast_logging_proto_lib',
      ],
      'sources': [
        'logging_defines.cc',
        'logging_defines.h',
        'logging_impl.cc',
        'logging_impl.h',
        'logging_raw.cc',
        'logging_raw.h',
        'logging_stats.cc',
        'logging_stats.h',
        'raw_event_subscriber.h',
        'simple_event_subscriber.cc',
        'simple_event_subscriber.h',
      ], # source
    },
    {
      'target_name': 'sender_logging',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_common_logging',
        'cast_logging_proto_lib',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        'cast_logging_proto_lib',
      ],
      'sources': [
        'encoding_event_subscriber.cc',
        'encoding_event_subscriber.h',
        'log_serializer.cc',
        'log_serializer.h',
      ], # source
    },
    {
      'target_name': 'cast_logging_proto_lib',
      'type': 'static_library',
      'sources': [
        'proto/proto_utils.cc',
        'proto/raw_events.proto',
      ],
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'media/cast/logging/proto',
      },
      'includes': ['../../../build/protoc.gypi'],
    },
    {
      'target_name': 'cast_log_analysis',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_logging_proto_lib',
        'sender_logging',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        'cast_logging_proto_lib',
      ],
      'sources': [
        'log_deserializer.cc',
        'log_deserializer.h',
      ], # source
    },
  ],
}
