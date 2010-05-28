# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    # Keep the archive builder happy.
    'chrome_personalization%': 1,
    'use_syncapi_stub%': 0,

    'conditions': [
      ['OS=="win"', {
        'python': [
          '<(DEPTH)\\third_party\\python_24\\setup_env.bat && python'
        ],
      }, { # OS != win
        'python': [
          'python'
        ],
      }],
    ],
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      # all our own includes are relative to src/
      '..',
    ],
  },
  'targets': [
    {
      'target_name': 'chrome_launcher',
      'type': 'executable',
      'msvs_guid': 'B7E540C1-49D9-4350-ACBC-FB8306316D16',
      'dependencies': [
        '../breakpad/breakpad.gyp:breakpad_handler',
      ],
      'sources': [
        'chrome_launcher_main.cc',
        'chrome_launcher.cc',
        'chrome_launcher.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile':
              '$(OutDir)\\servers\\$(ProjectName).exe',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
          'AdditionalDependencies': [
            'shlwapi.lib',
          ],
          
        },
      },
      'configurations': {
        'Release_Base': {
          # Set flags to unconditionally optimize chrome_frame_launcher.exe
          # for release builds.
          'msvs_settings': {
            'VCLinkerTool': {
              'LinkTimeCodeGeneration': '1',
            },
            'VCCLCompilerTool': {
              'Optimization': '3',
              'InlineFunctionExpansion': '2',
              'EnableIntrinsicFunctions': 'true',
              'FavorSizeOrSpeed': '2',
              'OmitFramePointers': 'true',
              'EnableFiberSafeOptimizations': 'true',
              'WholeProgramOptimization': 'true',
            },
            'VCLibrarianTool': {
              'AdditionalOptions': ['/ltcg', '/expectedoutputsize:120000000'],
            },
          },
        },
      },
    },
  ],
}
