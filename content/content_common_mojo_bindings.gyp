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
          'common/frame.mojom',
          'common/image_downloader/image_downloader.mojom',
          'common/leveldb_wrapper.mojom',
          'common/process_control.mojom',
          'common/render_frame_message_filter.mojom',
          'common/service_worker/embedded_worker_setup.mojom',
          'common/storage_partition_service.mojom',
          'common/websocket.mojom',
          '../third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom',
        ],
        'mojom_typemaps': [
          'common/bluetooth/web_bluetooth_device_id.typemap',
          '../device/bluetooth/public/interfaces/bluetooth_uuid.typemap',
          '../skia/public/interfaces/skbitmap.typemap',
          '../ui/gfx/geometry/mojo/geometry.typemap',
          '../url/mojo/gurl.typemap',
          '../url/mojo/origin.typemap',
        ],
      },
      'dependencies': [
        '../components/leveldb/leveldb.gyp:leveldb_bindings_mojom',
        '../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../services/shell/shell_public.gyp:shell_public',
        '../skia/skia.gyp:skia_mojo',
        '../third_party/WebKit/public/blink.gyp:mojo_bindings',
        '../ui/gfx/gfx.gyp:mojo_geometry_bindings',
      ],
      'includes': [ '../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'content_common_mojo_bindings',
      'type': 'static_library',
      'variables': {
        'enable_wexit_time_destructors': 1,
      },
      'dependencies': [
        '../device/sensors/sensors.gyp:device_sensors_mojo_bindings',
        '../url/url.gyp:url_mojom',
        '../skia/skia.gyp:skia',
        'content_common_mojo_bindings_mojom',
      ],
      'export_dependent_settings': [
        '../skia/skia.gyp:skia',
        '../url/url.gyp:url_mojom',
      ],
    },
  ]
}
