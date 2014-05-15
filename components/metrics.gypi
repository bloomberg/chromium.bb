# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'metrics',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/metrics_hashes.cc',
        'metrics/metrics_hashes.h',
        'metrics/metrics_log_base.cc',
        'metrics/metrics_log_base.h',
        'metrics/metrics_log_manager.cc',
        'metrics/metrics_log_manager.h',
        'metrics/metrics_pref_names.cc',
        'metrics/metrics_pref_names.h',
        'metrics/persisted_logs.cc',
        'metrics/persisted_logs.h',
      ],
    },
    {
      # Protobuf compiler / generator for UMA (User Metrics Analysis).
      'target_name': 'component_metrics_proto',
      'type': 'static_library',
      'sources': [
        'metrics/proto/chrome_user_metrics_extension.proto',
        'metrics/proto/histogram_event.proto',
        'metrics/proto/omnibox_event.proto',
        'metrics/proto/perf_data.proto',
        'metrics/proto/profiler_event.proto',
        'metrics/proto/sampled_profile.proto',
        'metrics/proto/system_profile.proto',
        'metrics/proto/user_action_event.proto',
      ],
      'variables': {
        'proto_in_dir': 'metrics/proto',
        'proto_out_dir': 'components/metrics/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
}
