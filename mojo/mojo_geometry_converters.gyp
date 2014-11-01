# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo/converters/geometry
      'target_name': 'mojo_geometry_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_GEOMETRY_IMPLEMENTATION',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'services/public/mojo_services_public.gyp:mojo_geometry_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'converters/geometry/geometry_type_converters.cc',
        'converters/geometry/geometry_type_converters.h',
        'converters/geometry/mojo_geometry_export.h',
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
