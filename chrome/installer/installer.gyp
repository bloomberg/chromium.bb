{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
  },
  'includes': [
    # Two versions of installer_util target are defined in installer_util.gypi.
    # This allows to keep all the settings relevant to these targets in one
    # place.
    'installer_util.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gcapi_dll',
          'type': 'loadable_module',
          'msvs_guid': 'B802A2FE-E4E2-4F5A-905A-D5128875C954',
          'dependencies': [
            '../../google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'gcapi/gcapi.cc',
            'gcapi/gcapi.h',
          ],
        },
        {
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'msvs_guid': 'CD2FD73A-6AAB-4886-B887-760D18E8B635',
          'dependencies': [
            '../../google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'gcapi/gcapi.cc',
            'gcapi/gcapi.h',
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
            '../..',
          ],
          'sources': [
            'gcapi/gcapi_test.cc',
            'gcapi/gcapi_test.rc',
            'gcapi/resource.h',
          ],
        },
        {
          'target_name': 'installer_util_unittests',
          'type': 'executable',
          'msvs_guid': '903F8C1E-537A-4C9E-97BE-075147CBE769',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '../../base/base.gyp:base',
            '../../base/base.gyp:base_i18n',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'setup/compat_checks_unittest.cc',
            'setup/setup_constants.cc',
            'util/browser_distribution_unittest.cc',
            'util/copy_tree_work_item_unittest.cc',
            'util/create_dir_work_item_unittest.cc',
            'util/create_reg_key_work_item_unittest.cc',
            'util/delete_after_reboot_helper_unittest.cc',
            'util/delete_reg_value_work_item_unittest.cc',
            'util/delete_tree_work_item_unittest.cc',
            'util/google_chrome_distribution_unittest.cc',
            'util/helper_unittest.cc',
            'util/installer_util_unittests.rc',
            'util/installer_util_unittests_resource.h',
            'util/lzma_util_unittest.cc',
            'util/master_preferences_unittest.cc',
            'util/move_tree_work_item_unittest.cc',
            'util/run_all_unittests.cc',
            'util/set_reg_value_work_item_unittest.cc',
            'util/shell_util_unittest.cc',
            'util/work_item_list_unittest.cc',
            'util/version_unittest.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
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
              'inputs': [
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
                         'util/prebuild/create_string_rc.py',
                         '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings'],
              'message': 'Generating resources from <(RULE_INPUT_PATH)',
            },
          ],
          'sources': [
            '../app/generated_resources.grd',
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
            '../../base/base.gyp:base',
            '../../base/base.gyp:base_i18n',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            '../test/mini_installer_test/run_all_unittests.cc',
            '../test/mini_installer_test/chrome_mini_installer.cc',
            '../test/mini_installer_test/chrome_mini_installer.h',
            '../test/mini_installer_test/mini_installer_test_constants.cc',
            '../test/mini_installer_test/mini_installer_test_constants.h',
            '../test/mini_installer_test/mini_installer_test_util.cc',
            '../test/mini_installer_test/mini_installer_test_util.h',
            '../test/mini_installer_test/test.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
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
            '../../build/util/build_util.gyp:lastchange',
            '../../build/util/support/support.gyp:*',
            '../../build/win/system.gyp:cygwin',
          ],
          'include_dirs': [
            '../..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/setup',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/setup',
            ],
          },
          'sources': [
            'mini_installer/chrome.release',
            'setup/install.cc',
            'setup/install.h',
            'setup/setup_main.cc',
            'setup/setup.ico',
            'setup/setup.rc',
            'setup/setup_constants.cc',
            'setup/setup_constants.h',
            'setup/setup_exe_version.rc.version',
            'setup/setup_resource.h',
            'setup/setup_util.cc',
            'setup/setup_util.h',
            'setup/uninstall.cc',
            'setup/uninstall.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\setup\\setup.exe.manifest',
            },
          },
          'rules': [
            {
              'rule_name': 'setup_version',
              'extension': 'version',
              'variables': {
                'version_py': '../../chrome/tools/build/version.py',
                'template_input_path': 'setup/setup_exe_version.rc.version',
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
                'python', '<(version_py)',
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
                'scan_server_dlls_py' : '../tools/build/win/scan_server_dlls.py',
              },
              'inputs': [
                '<scan_server_dlls_py)',
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
            ['chrome_frame_define==1', {
              'dependencies': [
                '../../chrome_frame/chrome_frame.gyp:npchrome_frame',
              ],
            }],
            # TODO(mark):  <(branding_dir) should be defined by the
            # global condition block at the bottom of the file, but
            # this doesn't work due to the following issue:
            #
            #   http://code.google.com/p/gyp/issues/detail?id=22
            #
            # Remove this block once the above issue is fixed.
            [ 'branding == "Chrome"', {
              'variables': {
                 'branding_dir': '../app/theme/google_chrome',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_dir': '../app/theme/chromium',
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
            '../../base/base.gyp:base',
            '../../base/base.gyp:base_i18n',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'setup/run_all_unittests.cc',
            'setup/setup_util.cc',
            'setup/setup_util_unittest.cc',
          ],
        },
      ],
    }],
    ['OS=="linux" and branding=="Chrome"', {
      'variables': {
        # Always google_chrome since this only applies to branding==Chrome.
        'branding_dir': '../app/theme/google_chrome',
        'version' : '<!(python <(version_py) -f ../../chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
        'revision' : '<!(python ../../build/util/lastchange.py | cut -d "=" -f 2)',
        'packaging_files_common': [
          'linux/internal/common/apt.include',
          'linux/internal/common/default-app.template',
          'linux/internal/common/default-app-block.template',
          'linux/internal/common/desktop.template',
          'linux/internal/common/google-chrome/google-chrome.info',
          'linux/internal/common/installer.include',
          'linux/internal/common/postinst.include',
          'linux/internal/common/prerm.include',
          'linux/internal/common/repo.cron',
          'linux/internal/common/rpm.include',
          'linux/internal/common/rpmrepo.cron',
          'linux/internal/common/updater',
          'linux/internal/common/variables.include',
          'linux/internal/common/wrapper',
        ],
        'packaging_files_deb': [
          'linux/internal/debian/build.sh',
          'linux/internal/debian/changelog.template',
          'linux/internal/debian/control.template',
          'linux/internal/debian/debian.menu',
          'linux/internal/debian/postinst',
          'linux/internal/debian/postrm',
          'linux/internal/debian/prerm',
        ],
        'packaging_files_rpm': [
          'linux/internal/rpm/build.sh',
          'linux/internal/rpm/chrome.spec.template',
        ],
        'packaging_files_binaries': [
          # TODO(mmoss) Any convenient way to get all the relevant build
          # files? (e.g. all locales, resources, etc.)
          '<(PRODUCT_DIR)/chrome',
          '<(PRODUCT_DIR)/chrome.pak',
          '<(PRODUCT_DIR)/chrome_sandbox',
          '<(PRODUCT_DIR)/libffmpegsumo.so',
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
          }],
          ['target_arch=="x64"', {
            'deb_arch': 'amd64',
            'rpm_arch': 'x86_64',
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
            # Copy tools for generating packages from the build archive.
            {
              'destination': '<(PRODUCT_DIR)/installer/',
              'files': [
                'linux/internal/build_from_archive.sh',
              ]
            },
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
                '<(branding_dir)/product_logo_32.png',
                '<(branding_dir)/product_logo_48.png',
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
                'python', '<(version_py)',
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
            ['chromeos==0 and toolkit_views==0', {
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
            ['chromeos==0 and toolkit_views==0', {
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
            ['chromeos==0 and toolkit_views==0', {
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
            ['chromeos==0 and toolkit_views==0', {
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
            '../chrome.gyp:chrome',
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
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '../app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '../app/theme/chromium',
      },
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
