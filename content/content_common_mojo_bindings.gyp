# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_common_mojo_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          # NOTE: Sources duplicated in //content/common/BUILD.gn:mojo_bindings.
          'common/geolocation_service.mojom',
          'common/permission_service.mojom',
          'common/render_frame_setup.mojom',

          # NOTE: Sources duplicated in
          # //content/public/common/BUILD.gn:mojo_bindings.
          'public/common/mojo_geoposition.mojom',
          'public/common/permission_status.mojom',
        ],
      },
      'includes': [ '../mojo/public/tools/bindings/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'content_common_mojo_bindings',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'content_common_mojo_bindings_mojom',
        '../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../mojo/public/mojo_public.gyp:mojo_application_bindings',
        '../mojo/public/mojo_public.gyp:mojo_cpp_bindings',
      ]
    },
  ]
}
