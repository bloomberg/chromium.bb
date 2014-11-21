# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'app_installer_util',
          'type': 'static_library',
          'dependencies': [
            'common',
            'installer_util',
            'installer_util_strings',
            'launcher_support',
            'common_constants.gyp:common_constants',
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../net/net.gyp:net',
            '../third_party/omaha/omaha.gyp:omaha_extractor',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'win/app_installer_util.cc',
            'win/app_installer_util.h',
          ],
        },
        # TODO(jackhou): Add a version resource (using
        # version_resource_rules.gypi).
        {
          'target_name': 'app_installer',
          'type': 'executable',
          'dependencies': [
            'app_installer_util',
            '../base/base.gyp:base',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'win/app_installer_main.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'app_installer/win/app_installer.exe.manifest',
              ],
            },
          },
        },
        {
          'target_name': 'app_installer_unittests',
          'type': 'executable',
          'dependencies': [
            'app_installer_util',
            '../base/base.gyp:base',
            '../base/base.gyp:run_all_unittests',
            '../net/net.gyp:net_test_support',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'win/app_installer_util_unittest.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'app_installer/win/app_installer.exe.manifest',
              ],
            },
          },
        },
      ],
    }],
  ],
}
