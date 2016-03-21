# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //components/profile_serivce:lib
      'target_name': 'profile_service_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'profile_app.cc',
        'profile_app.h',
        'profile_service_impl.cc',
        'profile_service_impl.h',
        'user_id_map.cc',
        'user_id_map.h',
      ],
      'dependencies': [
        'profile_service_bindings',
        '../../base/base.gyp:base',
        '../../components/filesystem/filesystem.gyp:filesystem_lib',
        '../../components/leveldb/leveldb.gyp:leveldb_lib',
        '../../mojo/mojo_base.gyp:mojo_application_base',
        '../../mojo/mojo_base.gyp:tracing_service',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_platform_handle.gyp:platform_handle',
        '../../url/url.gyp:url_lib',
      ],
      'export_dependent_settings': [
        'profile_service_bindings',
      ],
    },
    {
      # GN version: //components/profile_service/public/interfaces
      'target_name': 'profile_service_bindings',
      'type': 'static_library',
      'dependencies': [
        'profile_service_bindings_mojom',
      ],
    },
    {
      'target_name': 'profile_service_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/profile.mojom',
        ],
      },
      'dependencies': [
        '../../components/filesystem/filesystem.gyp:filesystem_bindings_mojom',
        '../../components/leveldb/leveldb.gyp:leveldb_bindings_mojom',
      ],
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    }
  ],
}
