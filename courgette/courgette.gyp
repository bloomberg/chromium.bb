# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'courgette_lib_sources': [
      'adjustment_method.cc',
      'adjustment_method_2.cc',
      'adjustment_method.h',
      'assembly_program.cc',
      'assembly_program.h',
      'third_party/bsdiff.h',
      'third_party/bsdiff_apply.cc',
      'third_party/bsdiff_create.cc',
      'third_party/paged_array.h',
      'courgette.h',
      'crc.cc',
      'crc.h',
      'difference_estimator.cc',
      'difference_estimator.h',
      'disassembler.cc',
      'disassembler.h',
      'disassembler_win32_x86.cc',
      'disassembler_win32_x86.h',
      'encoded_program.cc',
      'encoded_program.h',
      'ensemble.cc',
      'ensemble.h',
      'ensemble_apply.cc',
      'ensemble_create.cc',
      'image_info.cc',
      'image_info.h',
      'memory_allocator.cc',
      'memory_allocator.h',
      'region.h',
      'simple_delta.cc',
      'simple_delta.h',
      'streams.cc',
      'streams.h',
      'win32_x86_generator.h',
      'win32_x86_patcher.h',
    ],
  },
  'targets': [
    {
      'target_name': 'courgette_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
      ],
      'sources': [
        '<@(courgette_lib_sources)'
      ],
    },
    {
      'target_name': 'courgette',
      'type': 'executable',
      'sources': [
        'courgette_tool.cc',
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'LargeAddressAware': 2,
        },
      },
    },
    {
      'target_name': 'courgette_minimal_tool',
      'type': 'executable',
      'sources': [
        'courgette_minimal_tool.cc',
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'courgette_unittests',
      'type': 'executable',
      'sources': [
        'adjustment_method_unittest.cc',
        'bsdiff_memory_unittest.cc',
        'difference_estimator_unittest.cc',
        'encoded_program_unittest.cc',
        'encode_decode_unittest.cc',
        'image_info_unittest.cc',
        'run_all_unittests.cc',
        'streams_unittest.cc',
        'third_party/paged_array_unittest.cc'
      ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        [ 'toolkit_uses_gtk == 1', {
          'dependencies': [
            # Workaround for gyp bug 69.
            # Needed to handle the #include chain:
            #   base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'courgette_fuzz',
      'type': 'executable',
      'sources': [
        'encoded_program_fuzz_unittest.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        [ 'toolkit_uses_gtk == 1', {
          'dependencies': [
            # Workaround for gyp bug 69.
            # Needed to handle the #include chain:
            #   base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'courgette_lib64',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base_nacl_win64',
            '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk64',
          ],
          'sources': [
            '<@(courgette_lib_sources)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'courgette64',
          'type': 'executable',
          'sources': [
            'courgette_tool.cc',
          ],
          'dependencies': [
            'courgette_lib64',
            '../base/base.gyp:base_nacl_win64',
          ],
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
