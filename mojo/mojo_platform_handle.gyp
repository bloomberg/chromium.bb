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
      # GN version: //mojo/platform_handle:platform_handle/platform_handle_impl
      'target_name': 'platform_handle',
      'type': '<(component)',
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'PLATFORM_HANDLE_IMPLEMENTATION'
      ],
      'sources': [
        'platform_handle/platform_handle.h',
        'platform_handle/platform_handle_functions.h',
        'platform_handle/platform_handle_functions.cc',
      ],
      'dependencies': [
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_public.gyp:mojo_cpp_bindings',
      ]
    }
  ]
}
