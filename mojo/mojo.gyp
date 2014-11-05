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
        'edk/mojo_edk.gyp:mojo_edk',
        'mojo_base.gyp:mojo_base',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'mojo_input_events_converters.gyp:mojo_input_events_lib',
        'mojo_js_unittests',
        'mojo_surface_converters.gyp:mojo_surfaces_lib',
        'mojo_surface_converters.gyp:mojo_surfaces_lib_unittests',
        'services/public/mojo_services_public.gyp:mojo_services_public',
        'public/mojo_public.gyp:mojo_public',
      ],
    },
    {
      # GN version: //mojo/bindings/js/tests:mojo_js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'edk/mojo_edk.gyp:mojo_common_test_support',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_base.gyp:mojo_js_bindings_lib',
        'public/mojo_public.gyp:mojo_environment_standalone',
        'public/mojo_public.gyp:mojo_public_test_interfaces',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'bindings/js/tests/run_js_tests.cc',
      ],
    },
  ],
}
