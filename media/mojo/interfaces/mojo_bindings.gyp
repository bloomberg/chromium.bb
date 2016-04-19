# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN version: //media/mojo/interfaces
      'target_name': 'platform_verification_mojo_bindings',
      'type': 'none',
      'sources': [
        'platform_verification.mojom',
      ],
      'includes': [ '../../../mojo/mojom_bindings_generator.gypi' ],
    },
    {
      'target_name': 'platform_verification_api',
      'type': 'static_library',
      'dependencies': [
        'platform_verification_mojo_bindings',
        '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../../services/shell/shell.gyp:shell_public',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/media/mojo/interfaces/platform_verification.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/media/mojo/interfaces/platform_verification.mojom.h',
      ],
    },
    {
      # GN version: //media/mojo/interfaces
      'target_name': 'provision_fetcher_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'provision_fetcher.mojom',
        ],
      },
      'includes': [ '../../../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'provision_fetcher_api',
      'type': 'static_library',
      'dependencies': [
        'provision_fetcher_mojo_bindings',
        '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../../services/shell/shell.gyp:shell_public',
      ],
    },
  ],
}
