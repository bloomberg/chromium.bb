# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'tracing_service_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'services/tracing/public/interfaces/tracing.mojom',
        ],
        'mojom_include_path': '<(DEPTH)/mojo/services',
      },
      'includes': [
        'mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      # GN version: //mojo/services/tracing/public/interfaces
      'target_name': 'tracing_service_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        'tracing_service_bindings_mojom',
      ],
    },
  ],
}
