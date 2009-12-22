# Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
          'type': '<(library)',
          'sources': [
            'src/common/convert_UTF.c',
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
          ],
          'link_settings': {
            'libraries': ['$(SDKROOT)/usr/lib/libcrypto.dylib'],
          }
        },
        {
          'target_name': 'crash_inspector',
          'type': 'executable',
          'dependencies': [
            'breakpad_utilities',
          ],
          'sources': [
            'src/client/mac/crash_generation/Inspector.mm',
            'src/client/mac/crash_generation/InspectorMain.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'crash_report_sender',
          'type': 'executable',
          'mac_bundle': 1,
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/client/mac/sender/crash_report_sender.m',
            'src/common/mac/GTMLogger.m',
          ],
          'mac_bundle_resources': [
            'src/client/mac/sender/English.lproj/Localizable.strings',
            'src/client/mac/sender/crash_report_sender.icns',
            'src/client/mac/sender/Breakpad.nib',
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
          'include_dirs': [
            'src/common/mac',
          ],
          'dependencies': [
            'breakpad_utilities',
          ],
          'sources': [
            'src/common/dwarf/bytereader.cc',
            'src/common/dwarf/dwarf2reader.cc',
            'src/common/dwarf/functioninfo.cc',
            'src/common/mac/dump_syms.mm',
            'src/tools/mac/dump_syms/dump_syms_tool.mm',
          ],
          'xcode_settings': {
            # The DWARF utilities require -funsigned-char.
            'GCC_CHAR_IS_UNSIGNED_CHAR': 'YES',
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
          'type': '<(library)',
          'dependencies': [
            'breakpad_utilities',
            'crash_inspector',
            'crash_report_sender',
          ],
          'sources': [
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
        ['branding=="Chrome" or linux_breakpad==1', {
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

              'sources': [
                'src/common/linux/dump_symbols.cc',
                'src/common/linux/dump_symbols.h',
                'src/common/linux/file_id.cc',
                'src/common/linux/file_id.h',
                'src/common/linux/guid_creator.h',
                'src/common/linux/module.cc',
                'src/common/linux/module.h',
                'src/common/linux/stabs_reader.cc',
                'src/common/linux/stabs_reader.h',
                'src/tools/linux/dump_syms/dump_syms.cc',
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
          'type': '<(library)',

          'sources': [
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
            'src/common/linux/guid_creator.cc',
            'src/common/linux/guid_creator.h',
            'src/common/linux/linux_libc_support.h',
            'src/common/linux/linux_syscall_support.h',
            'src/common/linux/memory.h',
            'src/common/string_conversion.cc',
            'src/common/string_conversion.h',
          ],

          'include_dirs': [
            'src',
            '..',
            '.',
          ],
        },
        {
          'target_name': 'breakpad_unittests',
          'type': 'executable',
          'dependencies': [
            '../testing/gtest.gyp:gtest',
            '../testing/gtest.gyp:gtestmain',
            '../testing/gmock.gyp:gmock',
            'breakpad_client',
          ],

          'sources': [
            'src/client/linux/minidump_writer/directory_reader_unittest.cc',
            'src/client/linux/handler/exception_handler_unittest.cc',
            'src/client/linux/minidump_writer/line_reader_unittest.cc',
            'src/client/linux/minidump_writer/linux_dumper_unittest.cc',
            'src/common/linux/linux_libc_support_unittest.cc',
            'src/common/linux/memory_unittest.cc',
            'src/client/linux/minidump_writer/minidump_writer_unittest.cc',
            'linux/breakpad_googletest_includes.h',
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
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
