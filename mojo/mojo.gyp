# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'mojo_base.gyp:mojo_base',
        'mojo_edk_tests.gyp:mojo_edk_tests',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'mojo_input_events_converters.gyp:mojo_input_events_lib',
        'mojo_public.gyp:mojo_public',
        'mojo_services_public.gyp:mojo_services_public',
      ],
    },
  ]
}
