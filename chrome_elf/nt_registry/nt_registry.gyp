# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The nt_registry target should only depend on functions in kernel32.
# Please don't add dependencies on other system libraries.
{
  'target_defaults': {
    # This part is shared between the two versions of the target.
    'type': 'static_library',
    'sources': [
      'nt_registry.cc',
      'nt_registry.h',
    ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    'msvs_settings': {
      'VCLinkerTool': {
        # Please don't add dependencies on other system libraries.
        'AdditionalDependencies': [
          'kernel32.lib',
        ],
      },
    },
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # GN version: "//chrome_elf/nt_registry",
          'target_name': 'chrome_elf_nt_registry',
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # GN version: "//chrome_elf/nt_registry",
          'target_name': 'chrome_elf_nt_registry_nacl_win64',
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
