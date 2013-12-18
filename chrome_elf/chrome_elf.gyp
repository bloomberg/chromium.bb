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
    'blacklist.gypi',
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
        'blacklist',
        'chrome_elf_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'BaseAddress': '0x01c20000',
          # Set /SUBSYSTEM:WINDOWS for chrome_elf.dll (for consistency).
          'SubSystem': '2',
          # Exclude explicitly unwanted libraries from the link line.
          'IgnoreAllDefaultLibraries': 'true',
          'AdditionalDependencies!': [
            'user32.lib',
          ],
          'IgnoreDefaultLibraryNames': [
            'user32.lib',
          ],
        },
      },
    },
    {
      'target_name': 'chrome_elf_unittests',
      'type': 'executable',
      'sources': [
        'blacklist/test/blacklist_test.cc',
        'ntdll_cache_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'chrome_elf_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../sandbox/sandbox.gyp:sandbox',
        '../testing/gtest.gyp:gtest',
        'blacklist',
        'blacklist_test_dll_1',
        'blacklist_test_dll_2',
        'blacklist_test_dll_3',
        'blacklist_test_main_dll',
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
