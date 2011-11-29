# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'breakpad_sender.gypi',
    'breakpad_handler.gypi',
  ],
  'conditions': [
    [ 'OS=="mac"', {
      'target_defaults': {
        'include_dirs': [
          'src',
        ],
        'configurations': {
          'Debug_Base': {
            'defines': [
              # This is needed for GTMLogger to work correctly.
              'DEBUG',
            ],
          },
        },
      },
      'targets': [
        {
          'target_name': 'breakpad_utilities',
          'type': 'static_library',
          'sources': [
            'src/common/convert_UTF.c',
            'src/client/mac/handler/breakpad_nlist_64.cc',
            'src/client/mac/handler/dynamic_images.cc',
            'src/common/mac/file_id.cc',
            'src/common/mac/MachIPC.mm',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_walker.cc',
            'src/client/minidump_file_writer.cc',
            'src/client/mac/handler/minidump_generator.cc',
            'src/common/mac/SimpleStringDictionary.mm',
            'src/common/string_conversion.cc',
            'src/common/mac/string_utilities.cc',
            'src/common/md5.cc',
          ],
        },
        {
          'target_name': 'crash_inspector',
          'type': 'executable',
          'variables': {
            'mac_real_dsym': 1,
          },
          'dependencies': [
            'breakpad_utilities',
          ],
          'include_dirs': [
            'src/client/apple/Framework',
            'src/common/mac',
          ],
          'sources': [
            'src/client/mac/crash_generation/ConfigFile.mm',
            'src/client/mac/crash_generation/Inspector.mm',
            'src/client/mac/crash_generation/InspectorMain.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'crash_report_sender',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'mac_real_dsym': 1,
          },
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/client/mac/sender/crash_report_sender.m',
            'src/client/mac/sender/uploader.mm',
            'src/common/mac/GTMLogger.m',
          ],
          'mac_bundle_resources': [
            'src/client/mac/sender/English.lproj/Localizable.strings',
            'src/client/mac/sender/crash_report_sender.icns',
            'src/client/mac/sender/Breakpad.xib',
            'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'mac_bundle_resources!': [
             'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'xcode_settings': {
             'INFOPLIST_FILE': 'src/client/mac/sender/crash_report_sender-Info.plist',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ],
          }
        },
        {
          'target_name': 'dump_syms',
          'type': 'executable',
          'include_dirs++': [
            # ++ ensures this comes before src brought in from target_defaults.
            'pending/src',
          ],
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/dwarf/dwarf2diehandler.cc',
            'src/common/dwarf/dwarf2reader.cc',
            'src/common/dwarf/bytereader.cc',
            'src/common/dwarf_cfi_to_module.cc',
            'pending/src/common/dwarf_cu_to_module.cc',
            'src/common/dwarf_line_to_module.cc',
            'src/common/language.cc',
            'pending/src/common/module.cc',
            'src/common/mac/dump_syms.mm',
            'src/common/mac/file_id.cc',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_reader.cc',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_walker.cc',
            'src/common/stabs_reader.cc',
            'src/common/stabs_to_module.cc',
            'src/tools/mac/dump_syms/dump_syms_tool.mm',
            'src/common/md5.cc',
          ],
          'defines': [
            # For src/common/stabs_reader.h.
            'HAVE_MACH_O_NLIST_H',
          ],
          'xcode_settings': {
            # Like ld, dump_syms needs to operate on enough data that it may
            # actually need to be able to address more than 4GB. Use x86_64.
            # Don't worry! An x86_64 dump_syms is perfectly able to dump
            # 32-bit files.
            'ARCHS': [
              'x86_64',
            ],

            # The DWARF utilities require -funsigned-char.
            'GCC_CHAR_IS_UNSIGNED_CHAR': 'YES',

            # dwarf2reader.cc uses dynamic_cast.
            'GCC_ENABLE_CPP_RTTI': 'YES',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
          'configurations': {
            'Release_Base': {
              'xcode_settings': {
                # dump_syms crashes when built at -O1, -O2, and -O3.  It does
                # not crash at -Os.  To play it safe, dump_syms is always built
                # at -O0 until this can be sorted out.
                # http://code.google.com/p/google-breakpad/issues/detail?id=329
                'GCC_OPTIMIZATION_LEVEL': '0',  # -O0
               },
             },
          },
        },
        {
          'target_name': 'symupload',
          'type': 'executable',
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/tools/mac/symupload/symupload.m',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'breakpad',
          'type': 'static_library',
          'dependencies': [
            'breakpad_utilities',
            'crash_inspector',
            'crash_report_sender',
          ],
          'include_dirs': [
            'src/client/apple/Framework',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src/client/apple/Framework',
            ],
          },
          'defines': [
            'USE_PROTECTED_ALLOCATIONS=1',
          ],
          'sources': [
            'src/client/mac/crash_generation/crash_generation_client.cc',
            'src/client/mac/crash_generation/crash_generation_client.h',
            'src/client/mac/handler/protected_memory_allocator.cc',
            'src/client/mac/handler/exception_handler.cc',
            'src/client/mac/Framework/Breakpad.mm',
            'src/client/mac/Framework/OnDemandServer.mm',
          ],
        },
      ],
    }],
    [ 'OS=="linux"', {
      'conditions': [
        # Tools needed for archiving build symbols.
        ['linux_breakpad==1', {
          'targets': [
            {
              'target_name': 'symupload',
              'type': 'executable',

              # This uses the system libcurl, so don't use the default 32-bit
              # compile flags when building on a 64-bit machine.
              'variables': {
                'host_arch': '<!(uname -m)',
              },
              'conditions': [
                ['host_arch=="x86_64"', {
                  'cflags!': ['-m32', '-march=pentium4', '-msse2',
                              '-mfpmath=sse'],
                  'ldflags!': ['-m32'],
                  'cflags': ['-O2'],
                  'include_dirs!': ['/usr/include32'],
                }],
              ],

              'sources': [
                'src/tools/linux/symupload/sym_upload.cc',
                'src/common/linux/http_upload.cc',
                'src/common/linux/http_upload.h',
              ],
              'include_dirs': [
                'src',
              ],
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            },
            {
              'target_name': 'dump_syms',
              'type': 'executable',

              # dwarf2reader.cc uses dynamic_cast. Because we don't typically
              # don't support RTTI, we enable it for this single target. Since
              # dump_syms doesn't share any object files with anything else,
              # this doesn't end up polluting Chrome itself.
              'cflags_cc!': ['-fno-rtti'],

              'sources': [
                'src/common/dwarf/bytereader.cc',
                'src/common/dwarf_cfi_to_module.cc',
                'src/common/dwarf_cfi_to_module.h',
                'src/common/dwarf_cu_to_module.cc',
                'src/common/dwarf_cu_to_module.h',
                'src/common/dwarf/dwarf2diehandler.cc',
                'src/common/dwarf/dwarf2reader.cc',
                'src/common/dwarf_line_to_module.cc',
                'src/common/dwarf_line_to_module.h',
                'src/common/language.cc',
                'src/common/language.h',
                'src/common/linux/dump_symbols.cc',
                'src/common/linux/dump_symbols.h',
                'src/common/linux/elf_symbols_to_module.cc',
                'src/common/linux/elf_symbols_to_module.h',
                'src/common/linux/file_id.cc',
                'src/common/linux/file_id.h',
                'src/common/linux/guid_creator.h',
                'src/common/module.cc',
                'src/common/module.h',
                'src/common/stabs_reader.cc',
                'src/common/stabs_reader.h',
                'src/common/stabs_to_module.cc',
                'src/common/stabs_to_module.h',
                'src/tools/linux/dump_syms/dump_syms.cc',
              ],

              # Breakpad rev 583 introduced this flag.
              # Using this define, stabs_reader.h will include a.out.h to
              # build on Linux.
              'defines': [
                'HAVE_A_OUT_H',
              ],

              'include_dirs': [
                'src',
                '..',
              ],
            },
          ],
        }],
      ],
      'targets': [
        {
          'target_name': 'breakpad_client',
          'type': 'static_library',

          'sources': [
            'src/client/linux/crash_generation/crash_generation_client.cc',
            'src/client/linux/crash_generation/crash_generation_client.h',
            'src/client/linux/handler/exception_handler.cc',
            'src/client/linux/minidump_writer/directory_reader.h',
            'src/client/linux/minidump_writer/line_reader.h',
            'src/client/linux/minidump_writer/linux_dumper.cc',
            'src/client/linux/minidump_writer/linux_dumper.h',
            'src/client/linux/minidump_writer/minidump_writer.cc',
            'src/client/linux/minidump_writer/minidump_writer.h',
            'src/client/minidump_file_writer-inl.h',
            'src/client/minidump_file_writer.cc',
            'src/client/minidump_file_writer.h',
            'src/common/convert_UTF.c',
            'src/common/convert_UTF.h',
            'src/common/linux/file_id.cc',
            'src/common/linux/file_id.h',
            'src/common/linux/google_crashdump_uploader.cc',
            'src/common/linux/google_crashdump_uploader.h',
            'src/common/linux/guid_creator.cc',
            'src/common/linux/guid_creator.h',
            'src/common/linux/libcurl_wrapper.cc',
            'src/common/linux/libcurl_wrapper.h',
            'src/common/linux/linux_libc_support.h',
            'src/common/memory.h',
            'src/common/string_conversion.cc',
            'src/common/string_conversion.h',
          ],

          'conditions': [
            ['target_arch=="arm"', {
              'cflags': ['-Wa,-mimplicit-it=always'],
            }],
          ],

          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },

          'include_dirs': [
            'src',
            'src/client',
            'src/third_party/linux/include',
            '..',
            '.',
          ],
        },
        {
          # Breakpad r693 uses some files from src/processor in unit tests.
          'target_name': 'breakpad_processor_support',
          'type': 'static_library',

          'sources': [
            'src/processor/basic_code_modules.cc',
            'src/processor/basic_code_modules.h',
            'src/processor/logging.cc',
            'src/processor/logging.h',
            'src/processor/minidump.cc',
            'src/processor/pathname_stripper.cc',
            'src/processor/pathname_stripper.h',
          ],

          'include_dirs': [
            'src',
            'src/client',
            'src/third_party/linux/include',
            '..',
            '.',
          ],
        },
        {
          'target_name': 'breakpad_unittests',
          'type': 'executable',
          'dependencies': [
            '../testing/gtest.gyp:gtest',
            '../testing/gtest.gyp:gtest_main',
            '../testing/gmock.gyp:gmock',
            'breakpad_client',
            'breakpad_processor_support',
          ],

          'sources': [
            'linux/breakpad_googletest_includes.h',
            'src/client/linux/handler/exception_handler_unittest.cc',
            'src/client/linux/minidump_writer/directory_reader_unittest.cc',
            'src/client/linux/minidump_writer/line_reader_unittest.cc',
            'src/client/linux/minidump_writer/linux_dumper_unittest.cc',
            'src/client/linux/minidump_writer/minidump_writer_unittest.cc',
            'src/common/linux/file_id_unittest.cc',
            'src/common/linux/linux_libc_support_unittest.cc',
            'src/common/linux/synth_elf.cc',
            'src/common/memory_unittest.cc',
            'src/common/test_assembler.cc',
          ],

          'include_dirs': [
            'linux', # Use our copy of breakpad_googletest_includes.h
            'src',
            '..',
            '.',
          ],
        },
        {
          'target_name': 'generate_test_dump',
          'type': 'executable',

          'sources': [
            'linux/generate-test-dump.cc',
          ],

          'dependencies': [
            'breakpad_client',
          ],

          'include_dirs': [
            '..',
            'src',
          ],
        },
        {
          'target_name': 'minidump-2-core',
          'type': 'executable',

          'sources': [
            'src/tools/linux/md2core/minidump-2-core.cc'
          ],

          'include_dirs': [
            '..',
            'src',
          ],
        },
      ],
    }],
  ],
}
