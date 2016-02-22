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
      # GN version: //components/leveldb:lib
      'target_name': 'leveldb_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'env_mojo.cc',
        'env_mojo.h',
        'leveldb_database_impl.cc',
        'leveldb_database_impl.h',
        'leveldb_file_thread.cc',
        'leveldb_file_thread.h',
        'leveldb_service_impl.cc',
        'leveldb_service_impl.h',
        'util.cc',
        'util.h',
      ],
      'dependencies': [
        'leveldb_bindings_mojom',
        '../../components/filesystem/filesystem.gyp:filesystem_lib',
        '../../mojo/mojo_base.gyp:mojo_application_base',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
      ]
    },
    {
      # GN version: //components/leveldb/public/interfaces
      'target_name': 'leveldb_bindings',
      'type': 'static_library',
      'dependencies': [
        'leveldb_bindings_mojom',
      ],
    },
    {
      'target_name': 'leveldb_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/leveldb.mojom',
        ],
      },
      'dependencies': [
        '../../components/filesystem/filesystem.gyp:filesystem_bindings_mojom',
      ],
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    }
  ],
}
