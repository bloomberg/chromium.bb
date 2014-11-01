# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo/converters/surfaces
      'target_name': 'mojo_surfaces_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_SURFACES_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../gpu/gpu.gyp:gpu',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'sources': [
        'converters/surfaces/surfaces_type_converters.cc',
        'converters/surfaces/surfaces_type_converters.h',
        'converters/surfaces/surfaces_utils.cc',
        'converters/surfaces/surfaces_utils.h',
        'converters/surfaces/mojo_surfaces_export.h',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'dependencies': [
            'mojo_base.gyp:mojo_environment_chromium',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/converters/surfaces/tests
      'target_name': 'mojo_surfaces_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'mojo_surfaces_lib',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'sources': [
        'converters/surfaces/tests/surface_unittest.cc',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'dependencies': [
            'mojo_base.gyp:mojo_environment_chromium',
          ],
        }],
      ],
    },
  ],
}
