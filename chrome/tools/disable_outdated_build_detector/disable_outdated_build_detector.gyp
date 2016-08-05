# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'branding_dir': '../../../chrome/app/theme/<(branding_path_component)',
  },
  'includes': [
    '../../../build/util/version.gypi',
  ],
  'targets': [{
    'target_name': 'disable_outdated_build_detector_lib',
    'type': 'static_library',
    'dependencies': [
      '../../../base/base.gyp:base',
    ],
    'include_dirs': [
      '../../..',
    ],
    'sources': [
      'constants.cc',
      'constants.h',
      'disable_outdated_build_detector.cc',
      'disable_outdated_build_detector.h',
      'google_update_integration.cc',
      'google_update_integration.h',
    ],
  },{
    'target_name': 'disable_outdated_build_detector',
    'type': 'executable',
    'dependencies': [
      '../../../base/base.gyp:base',
      'disable_outdated_build_detector_lib',
    ],
    'include_dirs': [
      '../../..',
    ],
    'sources': [
      'disable_outdated_build_detector_exe_version.rc.version',
      'disable_outdated_build_detector_main.cc',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
      },
      'VCManifestTool': {
        'AdditionalManifestFiles': [
          '$(ProjectDir)\\disable_outdated_build_detector.exe.manifest',
        ],
      },
    },
    'rules': [
      {
        'rule_name': 'disable_outdated_build_detector_version',
        'extension': 'version',
        'variables': {
          'version_py_path': '<(DEPTH)/build/util/version.py',
          'template_input_path': '../../../chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector_exe_version.rc.version',
        },
        'inputs': [
          '<(template_input_path)',
          '<(version_path)',
          '<(lastchange_path)',
          '<(branding_dir)/BRANDING',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector_exe_version.rc',
        ],
        'action': [
          'python', '<(version_py_path)',
          '-f', '<(version_path)',
          '-f', '<(lastchange_path)',
          '-f', '<(branding_dir)/BRANDING',
          '<(template_input_path)',
          '<@(_outputs)',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Generating version information'
      },
    ],
    'conditions': [
      ['target_arch=="ia32"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'EnableEnhancedInstructionSet': '4',  # NoExtensions
          },
        },
      }],
    ],
  },{
    'target_name': 'disable_outdated_build_detector_unittests',
    'type': '<(gtest_target_type)',
    'dependencies': [
      'disable_outdated_build_detector_lib',
      '../../../base/base.gyp:base',
      '../../../base/base.gyp:test_support_base',
      '../../../chrome/chrome.gyp:installer_util',
      '../../../testing/gmock.gyp:gmock',
      '../../../testing/gtest.gyp:gtest',
    ],
    'include_dirs': [
      '../../..',
    ],
    'sources': [
      'disable_outdated_build_detector_unittest.cc',
      'google_update_integration_unittest.cc',
      'run_all_unittests.cc',
    ],
  },{
    'target_name': 'disable_outdated_build_detector_unittests_run',
    'type': 'none',
    'dependencies': [
      'disable_outdated_build_detector_unittests',
    ],
    'includes': [
      '../../../build/isolate.gypi',
    ],
    'sources': [
      'disable_outdated_build_detector_unittests.isolate',
    ],
  }],
}
