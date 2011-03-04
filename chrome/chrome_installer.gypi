# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gcapi_dll',
          'type': 'loadable_module',
          'msvs_guid': 'B802A2FE-E4E2-4F5A-905A-D5128875C954',
          'dependencies': [
            '<(DEPTH)/google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'installer/gcapi/gcapi.cc',
            'installer/gcapi/gcapi.h',
          ],
        },
        {
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'msvs_guid': 'CD2FD73A-6AAB-4886-B887-760D18E8B635',
          'dependencies': [
            '<(DEPTH)/google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'installer/gcapi/gcapi.cc',
            'installer/gcapi/gcapi.h',
          ],
        },
        {
          'target_name': 'gcapi_test',
          'type': 'executable',
          'msvs_guid': 'B64B396B-8EF1-4B6B-A07E-48D40EB961AB',
          'dependencies': [
            'gcapi_dll',
            'gcapi_lib',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'installer/gcapi/gcapi_test.cc',
            'installer/gcapi/gcapi_test.rc',
            'installer/gcapi/resource.h',
          ],
        },
        {
          'target_name': 'installer_util_unittests',
          'type': 'executable',
          'msvs_guid': '903F8C1E-537A-4C9E-97BE-075147CBE769',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/base.gyp:base_i18n',
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'installer/setup/compat_checks_unittest.cc',
            'installer/setup/setup_constants.cc',
            'installer/util/browser_distribution_unittest.cc',
            'installer/util/channel_info_unittest.cc',
            'installer/util/copy_tree_work_item_unittest.cc',
            'installer/util/create_dir_work_item_unittest.cc',
            'installer/util/create_reg_key_work_item_unittest.cc',
            'installer/util/delete_after_reboot_helper_unittest.cc',
            'installer/util/delete_reg_key_work_item_unittest.cc',
            'installer/util/delete_reg_value_work_item_unittest.cc',
            'installer/util/delete_tree_work_item_unittest.cc',
            'installer/util/google_chrome_distribution_unittest.cc',
            'installer/util/google_update_settings_unittest.cc',
            'installer/util/install_util_unittest.cc',
            'installer/util/installation_validator_unittest.cc',
            'installer/util/installation_validation_helper.cc',
            'installer/util/installation_validation_helper.h',
            'installer/util/installer_state_unittest.cc',
            'installer/util/installer_util_unittests.rc',
            'installer/util/installer_util_unittests_resource.h',
            'installer/util/language_selector_unittest.cc',
            'installer/util/lzma_util_unittest.cc',
            'installer/util/master_preferences_unittest.cc',
            'installer/util/move_tree_work_item_unittest.cc',
            'installer/util/product_unittest.h',
            'installer/util/product_unittest.cc',
            'installer/util/product_state_unittest.cc',
            'installer/util/run_all_unittests.cc',
            'installer/util/self_cleaning_temp_dir_unittest.cc',
            'installer/util/set_reg_value_work_item_unittest.cc',
            'installer/util/shell_util_unittest.cc',
            'installer/util/wmi_unittest.cc',
            'installer/util/work_item_list_unittest.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\installer\\mini_installer\\mini_installer.exe.manifest',
            },
          },
        },
        {
          'target_name': 'installer_util_strings',
          'msvs_guid': '0026A376-C4F1-4575-A1BA-578C69F07013',
          'type': 'none',
          'rules': [
            {
              'rule_name': 'installer_util_strings',
              'extension': 'grd',
              'variables': {
                'create_string_rc_py' : 'installer/util/prebuild/create_string_rc.py',
              },
              'inputs': [
                '<(create_string_rc_py)',
                '<(RULE_INPUT_PATH)',
              ],
              'outputs': [
                # Don't use <(RULE_INPUT_ROOT) to create the output file
                # name, because the base name of the input
                # (generated_resources.grd) doesn't match the generated file
                # (installer_util_strings.h).
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.h',
              ],
              'action': ['python',
                         '<(create_string_rc_py)',
                         '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings',
                         '<(branding)',],
              'message': 'Generating resources from <(RULE_INPUT_PATH)',
            },
          ],
          'sources': [
            'app/chromium_strings.grd',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings',
            ],
          },
        },
        {
          'target_name': 'mini_installer_test',
          'type': 'executable',
          'msvs_guid': '4B6E199A-034A-49BD-AB93-458DD37E45B1',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/base.gyp:base_i18n',
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/chrome/chrome.gyp:test_support_common',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.rc',
            'installer/util/installation_validation_helper.cc',
            'installer/util/installation_validation_helper.h',
            'test/mini_installer_test/run_all_unittests.cc',
            'test/mini_installer_test/chrome_mini_installer.cc',
            'test/mini_installer_test/chrome_mini_installer.h',
            'test/mini_installer_test/mini_installer_test_constants.cc',
            'test/mini_installer_test/mini_installer_test_constants.h',
            'test/mini_installer_test/mini_installer_test_util.cc',
            'test/mini_installer_test/mini_installer_test_util.h',
            'test/mini_installer_test/test.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\installer\\mini_installer\\mini_installer.exe.manifest',
            },
          },
        },
        {
          'target_name': 'setup',
          'type': 'executable',
          'msvs_guid': '21C76E6E-8B38-44D6-8148-B589C13B9554',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
            '<(DEPTH)/build/util/build_util.gyp:lastchange',
            '<(DEPTH)/build/util/support/support.gyp:*',
            '<(DEPTH)/build/win/system.gyp:cygwin',
            '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
            '<(DEPTH)/chrome_frame/chrome_frame.gyp:npchrome_frame',
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
            '<(DEPTH)/rlz/rlz.gyp:rlz_lib',
            '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
          ],
          'include_dirs': [
            '<(DEPTH)',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/setup',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/setup',
            ],
          },
          'sources': [
            'installer/mini_installer/chrome.release',
            'installer/setup/chrome_frame_quick_enable.cc',
            'installer/setup/chrome_frame_quick_enable.h',
            'installer/setup/chrome_frame_ready_mode.cc',
            'installer/setup/chrome_frame_ready_mode.h',
            'installer/setup/install.cc',
            'installer/setup/install.h',
            'installer/setup/install_worker.cc',
            'installer/setup/install_worker.h',
            'installer/setup/setup_main.cc',
            'installer/setup/setup.ico',
            'installer/setup/setup.rc',
            'installer/setup/setup_constants.cc',
            'installer/setup/setup_constants.h',
            'installer/setup/setup_exe_version.rc.version',
            'installer/setup/setup_resource.h',
            'installer/setup/setup_util.cc',
            'installer/setup/setup_util.h',
            'installer/setup/uninstall.cc',
            'installer/setup/uninstall.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\installer\\setup\\setup.exe.manifest',
            },
          },
          'rules': [
            {
              'rule_name': 'setup_version',
              'extension': 'version',
              'variables': {
                'version_py_path': '<(DEPTH)/chrome/tools/build/version.py',
                'template_input_path': 'installer/setup/setup_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/setup/setup_exe_version.rc',
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
            {
              'rule_name': 'server_dlls',
              'extension': 'release',
              'variables': {
                'scan_server_dlls_py' : 'tools/build/win/scan_server_dlls.py',
                'template_file': 'mini_installer/chrome.release',
              },
              'inputs': [
                '<(scan_server_dlls_py)',
                '<(template_file)'
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/registered_dlls.h',
              ],
              'action': [
                'python',
                '<(scan_server_dlls_py)',
                '--output_dir=<(PRODUCT_DIR)',
                '--input_file=<(RULE_INPUT_PATH)',
                '--header_output_dir=<(INTERMEDIATE_DIR)',
                # TODO(sgk):  may just use environment variables
                #'--distribution=$(CHROMIUM_BUILD)',
                '--distribution=_google_chrome',
              ],
            },
          ],
          'conditions': [
            # TODO(mark):  <(branding_dir) should be defined by the
            # global condition block at the bottom of the file, but
            # this doesn't work due to the following issue:
            #
            #   http://code.google.com/p/gyp/issues/detail?id=22
            #
            # Remove this block once the above issue is fixed.
            [ 'branding == "Chrome"', {
              'variables': {
                 'branding_dir': 'app/theme/google_chrome',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_dir': 'app/theme/chromium',
              },
            }],
          ],
        },
        {
          'target_name': 'setup_unittests',
          'type': 'executable',
          'msvs_guid': 'C0AE4E06-F023-460F-BC14-6302CEAC51F8',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/base.gyp:base_i18n',
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
            '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)',
            '<(INTERMEDIATE_DIR)',
          ],
          # TODO(robertshield): Move the items marked with "Move to lib"
          # below into a separate lib and then link both setup.exe and
          # setup_unittests.exe against that.
          'sources': [
            'installer/setup/install_worker.cc',    # Move to lib
            'installer/setup/install_worker.h',     # Move to lib
            'installer/setup/install_worker_unittest.cc',
            'installer/setup/run_all_unittests.cc',
            'installer/setup/setup_constants.cc',   # Move to lib
            'installer/setup/setup_constants.h',    # Move to lib
            'installer/setup/setup_unittests.rc',
            'installer/setup/setup_unittests_resource.h',
            'installer/setup/setup_util.cc',
            'installer/setup/setup_util_unittest.cc',
          ],
        },
      ],
    }],
    ['OS=="linux" and branding=="Chrome"', {
      'variables': {
        # Always google_chrome since this only applies to branding==Chrome.
        'branding_dir': 'app/theme/google_chrome',
        'version' : '<!(python <(version_py_path) -f <(DEPTH)/chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
        'revision' : '<!(python <(DEPTH)/build/util/lastchange.py --revision-only)',
        'packaging_files_common': [
          'installer/linux/internal/common/apt.include',
          'installer/linux/internal/common/default-app.template',
          'installer/linux/internal/common/default-app-block.template',
          'installer/linux/internal/common/desktop.template',
          'installer/linux/internal/common/google-chrome/google-chrome.info',
          'installer/linux/internal/common/installer.include',
          'installer/linux/internal/common/postinst.include',
          'installer/linux/internal/common/prerm.include',
          'installer/linux/internal/common/repo.cron',
          'installer/linux/internal/common/rpm.include',
          'installer/linux/internal/common/rpmrepo.cron',
          'installer/linux/internal/common/updater',
          'installer/linux/internal/common/variables.include',
          'installer/linux/internal/common/wrapper',
        ],
        'packaging_files_deb': [
          'installer/linux/internal/debian/build.sh',
          'installer/linux/internal/debian/changelog.template',
          'installer/linux/internal/debian/control.template',
          'installer/linux/internal/debian/debian.menu',
          'installer/linux/internal/debian/postinst',
          'installer/linux/internal/debian/postrm',
          'installer/linux/internal/debian/prerm',
        ],
        'packaging_files_rpm': [
          'installer/linux/internal/rpm/build.sh',
          'installer/linux/internal/rpm/chrome.spec.template',
        ],
        'packaging_files_binaries': [
          # TODO(mmoss) Any convenient way to get all the relevant build
          # files? (e.g. all locales, resources, etc.)
          '<(PRODUCT_DIR)/chrome',
          '<(PRODUCT_DIR)/chrome.pak',
          '<(PRODUCT_DIR)/chrome_sandbox',
          '<(PRODUCT_DIR)/libffmpegsumo.so',
          '<(PRODUCT_DIR)/libppGoogleNaClPluginChrome.so',
          '<(PRODUCT_DIR)/xdg-mime',
          '<(PRODUCT_DIR)/xdg-settings',
          '<(PRODUCT_DIR)/locales/en-US.pak',
        ],
        'flock_bash': ['flock', '--', '/tmp/linux_package_lock', 'bash'],
        'deb_build': '<(PRODUCT_DIR)/installer/debian/build.sh',
        'rpm_build': '<(PRODUCT_DIR)/installer/rpm/build.sh',
        'deb_cmd': ['<@(flock_bash)', '<(deb_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'rpm_cmd': ['<@(flock_bash)', '<(rpm_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'conditions': [
          ['target_arch=="ia32"', {
            'deb_arch': 'i386',
            'rpm_arch': 'i386',
            # Flash Player for Linux is currently only available for ia32.
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/libgcflashplayer.so',
              '<(PRODUCT_DIR)/plugin.vch',
            ],
          }],
          ['target_arch=="x64"', {
            'deb_arch': 'amd64',
            'rpm_arch': 'x86_64',
          }],
          ['target_arch=="arm"', {
            'deb_arch': 'arm',
            'rpm_arch': 'arm',
          }],
          ['internal_pdf', {
            'packaging_files_binaries': [
              '<(PRODUCT_DIR)/libpdf.so',
            ],
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'linux_installer_configs',
          'type': 'none',
          # Add these files to the build output so the build archives will be
          # "hermetic" for packaging. This is only for branding="Chrome" since
          # we only create packages for official builds.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/installer/debian/',
              'files': [
                '<@(packaging_files_deb)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/installer/rpm/',
              'files': [
                '<@(packaging_files_rpm)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/installer/common/',
              'files': [
                '<@(packaging_files_common)',
              ]
            },
            # Additional theme resources needed for package building.
            {
              'destination': '<(PRODUCT_DIR)/installer/theme/',
              'files': [
                '<(branding_dir)/product_logo_16.png',
                '<(branding_dir)/product_logo_22.png',
                '<(branding_dir)/product_logo_24.png',
                '<(branding_dir)/product_logo_32.png',
                '<(branding_dir)/product_logo_48.png',
                '<(branding_dir)/product_logo_64.png',
                '<(branding_dir)/product_logo_128.png',
                '<(branding_dir)/product_logo_256.png',
                '<(branding_dir)/product_logo_32.xpm',
                '<(branding_dir)/BRANDING',
              ],
            },
          ],
          'actions': [
            {
              'action_name': 'save_build_info',
              'inputs': [
                '<(branding_dir)/BRANDING',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/installer/version.txt',
              ],
              # Just output the default version info variables.
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(branding_dir)/BRANDING',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-o', '<@(_outputs)'
              ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_all',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_unstable',
            'linux_packages_beta',
            'linux_packages_stable',
          ],
        },
        {
          # 'trunk' is a developer, testing-only package, so it shouldn't be
          # included in the 'linux_packages_all' collection.
          'target_name': 'linux_packages_trunk',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_trunk_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_trunk_rpm',
              ],
            }],
          ],
        },
        {
          'target_name': 'linux_packages_unstable',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_unstable_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_unstable_rpm',
              ],
            }],
          ],
        },
        {
          'target_name': 'linux_packages_beta',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_beta_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_beta_rpm',
              ],
            }],
          ],
        },
        {
          'target_name': 'linux_packages_stable',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_packages_stable_deb',
          ],
          # ChromeOS doesn't care about RPM packages.
          'conditions': [
            ['chromeos==0', {
              'dependencies': [
                'linux_packages_stable_rpm',
              ],
            }],
          ],
        },
        # TODO(mmoss) gyp looping construct would be handy here ...
        # These package actions are the same except for the 'channel' variable.
        {
          'target_name': 'linux_packages_trunk_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'trunk',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_unstable_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'unstable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_beta_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_stable_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'stable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_trunk_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'trunk',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_unstable_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'unstable',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_beta_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_packages_stable_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'stable',
              },
              'action_name': 'rpm_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'variables': {
        'mac_packaging_dir':
            '<(PRODUCT_DIR)/<(mac_product_name) Packaging',
        # <(PRODUCT_DIR) expands to $(BUILT_PRODUCTS_DIR), which doesn't
        # work properly in a shell script, where ${BUILT_PRODUCTS_DIR} is
        # needed.
        'mac_packaging_sh_dir':
            '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Packaging',
      }, # variables
      'targets': [
        {
          'target_name': 'installer_packaging',
          'type': 'none',
          'dependencies': [
            'installer/mac/third_party/bsdiff/goobsdiff.gyp:*',
            'installer/mac/third_party/xz/xz.gyp:*',
          ],
          'conditions': [
            ['buildtype=="Official"', {
              'actions': [
                {
                  # Create sign.sh, the script that the packaging system will
                  # use to sign the .app bundle.
                  'action_name': 'Make sign.sh',
                  'variables': {
                    'make_signers_sh_path': 'installer/mac/make_signers.sh',
                  },
                  'inputs': [
                    '<(make_signers_sh_path)',
                    'installer/mac/sign_app.sh.in',
                    'installer/mac/sign_versioned_dir.sh.in',
                    'installer/mac/app_resource_rules.plist.in',
                    '<(version_path)',
                  ],
                  'outputs': [
                    '<(mac_packaging_dir)/sign_app.sh',
                    '<(mac_packaging_dir)/sign_versioned_dir.sh',
                    '<(mac_packaging_dir)/app_resource_rules.plist',
                  ],
                  'action': [
                    '<(make_signers_sh_path)',
                    '<(mac_packaging_sh_dir)',
                    '<(mac_product_name)',
                    '<(version_full)',
                  ],
                },
              ],  # actions
            }],  # buildtype=="Official"
          ],  # conditions
          'copies': [
            {
              # Put the files where the packaging system will find them. The
              # packager will use these when building the "full installer"
              # disk images and delta/differential update disk images.
              'destination': '<(mac_packaging_dir)',
              'files': [
                '<(PRODUCT_DIR)/goobsdiff',
                '<(PRODUCT_DIR)/goobspatch',
                '<(PRODUCT_DIR)/liblzma_decompress.dylib',
                '<(PRODUCT_DIR)/xz',
                '<(PRODUCT_DIR)/xzdec',
                'installer/mac/dirdiffer.sh',
                'installer/mac/dirpatcher.sh',
                'installer/mac/dmgdiffer.sh',
                'installer/mac/pkg-dmg',
              ],
              'conditions': [
                ['mac_keystone==1', {
                  'files': [
                    'installer/mac/keystone_install.sh',
                  ],
                }],  # mac_keystone
                ['branding=="Chrome" and buildtype=="Official"', {
                  'files': [
                    'installer/mac/internal/chrome_dmg_background.png',
                    'installer/mac/internal/chrome_dmg_dsstore',
                    'installer/mac/internal/chrome_dmg_icon.icns',
                    'installer/mac/internal/generate_dmgs',
                  ],
                }],  # branding=="Chrome" and buildtype=="Official"
              ],  # conditions
            },
          ],  # copies
        },  # target: installer_packaging
      ],  # targets
    }],  # OS=="mac"
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': 'app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': 'app/theme/chromium',
      },
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
