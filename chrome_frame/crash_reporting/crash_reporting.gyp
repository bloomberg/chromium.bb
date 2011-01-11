# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      # all our own includes are relative to src/
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'crash_report',
      'type': 'static_library',
      'sources': [
        'crash_metrics.cc',
        'crash_metrics.h',
        'crash_report.cc',
        'crash_report.h',
        'nt_loader.cc',
        'nt_loader.h',
        'vectored_handler-impl.h',
        'vectored_handler.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../../breakpad/breakpad.gyp:breakpad_handler',
          ],
        }],
      ],
    },
    {
      'target_name': 'crash_dll',
      'type': 'loadable_module',
      'sources': [
        'crash_dll.cc',
        'crash_dll.h',
      ],
      'msvs_settings': {
        # To work around a bug in some versions of the CRT, which cause
        # crashes on program exit if a DLL crashes at process attach time,
        # we cut out the CRT entirely, and set our DLL main routine as the
        # entry point for the DLL.
        'VCLinkerTool': {
          'EntryPointSymbol': 'DllMain',
          'IgnoreAllDefaultLibraries': 'true',
        },
        # Turn off buffer security checks, since we don't have CRT
        # support for them, given that we don't link the CRT.
        'VCCLCompilerTool': {
          'BufferSecurityCheck': 'false',
        },
      },
      'configurations': {
        'Debug': {
          'msvs_settings': {
            # Turn off basic CRT checks, since we don't have CRT support.
            # We have to do this per configuration, as base.gypi specifies
            # this per-config, which binds tighter than the defaults above.
            'VCCLCompilerTool': {
              'BasicRuntimeChecks': '0',
            },
          },
        },
        'Debug_x64': {
          'msvs_settings': {
            # Turn off basic CRT checks, since we don't have CRT support.
            # We have to do this per configuration, as base.gypi specifies
            # this per-config, which binds tighter than the defaults above.
            'VCCLCompilerTool': {
              'BasicRuntimeChecks': '0',
            },
          },
        },
      },
    },
    {
      'target_name': 'vectored_handler_tests',
      'type': 'executable',
      'sources': [
        'nt_loader_unittest.cc',
        'vectored_handler_unittest.cc',
        'veh_test.cc',
        'veh_test.h',
      ],
      'dependencies': [
        'crash_dll',
        'crash_report',
        '../../base/base.gyp:base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        '../../breakpad/breakpad.gyp:breakpad_handler',
      ],
    },
    {
      'target_name': 'minidump_test',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'minidump_test.cc',
      ],
    },
  ],
}

# vim: shiftwidth=2:et:ai:tabstop=2

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
