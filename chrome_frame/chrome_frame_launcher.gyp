# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    # Keep the archive builder happy.
    'chrome_personalization%': 1,
    'use_syncapi_stub%': 0,

    'variables': {
      'version_py_path': '../tools/build/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path) -f',
    'version_path': '<(version_path)',

    'conditions': [
      ['OS=="win"', {
        'python': [
          '<(DEPTH)\\third_party\\python_26\\setup_env.bat && python'
        ],
      }, { # OS != win
        'python': [
          'python'
        ],
      }],
    ],
  },
  'target_defaults': {
    'include_dirs': [
      # all our own includes are relative to src/
      '..',
    ],
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
  'targets': [
    {
      'target_name': 'chrome_launcher',
      'type': 'executable',
      'dependencies': [
        '../breakpad/breakpad.gyp:breakpad_handler',
        '../chrome/chrome.gyp:chrome_version_header',
        '../google_update/google_update.gyp:google_update',
        'chrome_frame.gyp:chrome_frame_utils',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'chrome_launcher_main.cc',
        'chrome_launcher_version.rc',
        'chrome_launcher.cc',
        'chrome_launcher.h',
        'update_launcher.cc',
        'update_launcher.h'
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
          'AdditionalDependencies': [
            'shlwapi.lib',
          ],
        },
      },
    },
    {
      'target_name': 'chrome_frame_helper',
      'type': 'executable',
      'dependencies': [
        '../breakpad/breakpad.gyp:breakpad_handler',
        '../chrome/chrome.gyp:chrome_version_header',
        'chrome_frame.gyp:chrome_frame_utils',
        'chrome_frame_helper_dll',
        'chrome_frame_helper_lib',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../chrome_frame',
      ],
      'sources': [
        'chrome_frame_helper_main.cc',
        'chrome_frame_helper_version.rc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile':
              '$(OutDir)\\$(ProjectName).exe',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
        },
      },
    },
    {
      'target_name': 'chrome_frame_helper_dll',
      'type': 'shared_library',
      'dependencies': [
        '../chrome/chrome.gyp:chrome_version_header',
        'chrome_frame_helper_lib',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../chrome_frame',
      ],
      'sources': [
        'bho_loader.cc',
        'bho_loader.h',
        'chrome_frame_helper_dll.cc',
        'chrome_frame_helper_dll.def',
        'chrome_frame_helper_version.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'chrome_tab.idl',
        'event_hooker.cc',
        'event_hooker.h',
        'iids.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\chrome_frame_helper.dll',
          'ProgramDatabaseFile': '$(OutDir)\\chrome_frame_helper_dll.pdb',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
        },
      },
    },
    {
      'target_name': 'chrome_frame_helper_lib',
      'type': 'static_library',
      'dependencies': [
        '../chrome/chrome.gyp:chrome_version_header',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../chrome_frame',
      ],
      'sources': [
        'chrome_frame_helper_util.cc',
        'chrome_frame_helper_util.h',
        'registry_watcher.cc',
        'registry_watcher.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'chrome_tab.idl',
        'iids.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'shlwapi.lib',
          ],
        },
      },
    },
  ],
}
