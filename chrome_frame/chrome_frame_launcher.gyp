# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  'includes': [
    '../chrome/version.gypi',
  ],
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
      'target_name': 'chrome_frame_launcher_version_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'chrome_frame',
        'template_input_path': 'chrome_frame_version.rc.version',
        'extra_variable_files_arguments': [ '-f', 'BRANDING' ],
        'extra_variable_files': [ 'BRANDING' ], # NOTE: matches that above
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)',
        ],
      },
      'sources': [
        'chrome_frame_helper_dll.ver',
        'chrome_frame_helper_exe.ver',
        'chrome_launcher_exe.ver',
      ],
      'includes': [
        '../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'chrome_launcher',
      'type': 'executable',
      'dependencies': [
        '../breakpad/breakpad.gyp:breakpad_handler',
        '../chrome/app/policy/cloud_policy_codegen.gyp:policy',
        '../google_update/google_update.gyp:google_update',
        'chrome_frame.gyp:chrome_frame_utils',
        'chrome_frame_launcher_version_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_launcher_exe_version.rc',
        'chrome_launcher_main.cc',
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
        'chrome_frame.gyp:chrome_frame_utils',
        'chrome_frame_helper_dll',
        'chrome_frame_helper_lib',
        'chrome_frame_launcher_version_resources',
      ],
      'sources': [
        'chrome_frame_helper_main.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_frame_helper_exe_version.rc',
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
        'chrome_frame.gyp:chrome_tab_idl',
        'chrome_frame_helper_lib',
        'chrome_frame_launcher_version_resources',
      ],
      'sources': [
        'bho_loader.cc',
        'bho_loader.h',
        'chrome_frame_helper_dll.cc',
        'chrome_frame_helper_dll.def',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_frame_helper_dll_version.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_tab.h',
        'event_hooker.cc',
        'event_hooker.h',
        'iids.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\chrome_frame_helper.dll',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
        },
      },
    },
    {
      'target_name': 'chrome_frame_helper_lib',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame.gyp:chrome_tab_idl',
      ],
      'sources': [
        'chrome_frame_helper_util.cc',
        'chrome_frame_helper_util.h',
        'registry_watcher.cc',
        'registry_watcher.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_tab.h',
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
