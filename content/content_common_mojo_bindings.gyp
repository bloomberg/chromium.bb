# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //content/common:mojo_bindings
      'target_name': 'content_common_mojo_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          # NOTE: Sources duplicated in //content/common/BUILD.gn:mojo_bindings.
          'common/application_setup.mojom',
          'common/background_sync_service.mojom',
          'common/geolocation_service.mojom',
          'common/image_downloader/image_downloader.mojom',
          'common/permission_service.mojom',
          'common/presentation/presentation_service.mojom',
          'common/process_control.mojom',
          'common/render_frame_setup.mojom',
          'common/service_port_service.mojom',
          'common/service_worker/embedded_worker_setup.mojom',
          'common/vr_service.mojom',
          'common/wake_lock_service.mojom',

          # NOTE: Sources duplicated in
          # //content/public/common/BUILD.gn:mojo_bindings.
          'public/common/background_sync.mojom',
          'public/common/mojo_geoposition.mojom',
          'public/common/permission_status.mojom',
          'public/common/service_worker_event_status.mojom',
        ],
      },
      'includes': [ '../third_party/mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'content_common_mojo_bindings',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'content_common_mojo_bindings_mojom',
        '../mojo/mojo_base.gyp:mojo_application_bindings',
        '../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../skia/skia.gyp:skia_mojo',
        '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../ui/mojo/geometry/mojo_bindings.gyp:mojo_geometry_bindings',
      ]
    },
  ]
}
