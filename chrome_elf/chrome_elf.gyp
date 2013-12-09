# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
    '../chrome/version.gypi',
  ],
  'targets': [
    {
      'target_name': 'chrome_elf',
      'type': 'shared_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'chrome_elf.def',
        'chrome_elf_main.cc',
        'chrome_elf_main.h',
      ],
      'dependencies': [
        'chrome_elf_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'BaseAddress': '0x01c20000',
          # Set /SUBSYSTEM:WINDOWS for chrome_elf.dll (for consistency).
          'SubSystem': '2',
        },
      },
    },
    {
      'target_name': 'chrome_elf_unittests',
      'type': 'executable',
      'sources': [
        'ntdll_cache_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'chrome_elf_lib',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'chrome_elf_lib',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'chrome_elf_types.h',
        'ntdll_cache.cc',
        'ntdll_cache.h',
      ],
    },
  ],
}
