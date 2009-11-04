{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'installer_util',
          'type': '<(library)',
          'msvs_guid': 'EFBB1436-A63F-4CD8-9E99-B89226E782EC',
          'dependencies': [
            '../../app/app.gyp:app_id',
            'installer_util_strings',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:chrome_resources',
            '../chrome.gyp:chrome_strings',
            '../../courgette/courgette.gyp:courgette_lib',
            '../../third_party/bspatch/bspatch.gyp:bspatch',
            '../../third_party/icu/icu.gyp:icui18n',
            '../../third_party/icu/icu.gyp:icuuc',
            '../../third_party/libxml/libxml.gyp:libxml',
            '../../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'util/browser_distribution.cc',
            'util/browser_distribution.h',
            'util/chrome_frame_distribution.cc',
            'util/chrome_frame_distribution.h',
            'util/compat_checks.cc',
            'util/compat_checks.h',
            'util/copy_tree_work_item.cc',
            'util/copy_tree_work_item.h',
            'util/create_dir_work_item.cc',
            'util/create_dir_work_item.h',
            'util/create_reg_key_work_item.cc',
            'util/create_reg_key_work_item.h',
            'util/delete_after_reboot_helper.cc',
            'util/delete_after_reboot_helper.h',
            'util/delete_reg_value_work_item.cc',
            'util/delete_reg_value_work_item.h',
            'util/delete_tree_work_item.cc',
            'util/delete_tree_work_item.h',
            'util/google_chrome_distribution.cc',
            'util/google_chrome_distribution.h',
            'util/google_update_constants.cc',
            'util/google_update_constants.h',
            'util/google_update_settings.cc',
            'util/google_update_settings.h',
            'util/helper.cc',
            'util/helper.h',
            'util/html_dialog.h',
            'util/html_dialog_impl.cc',
            'util/install_util.cc',
            'util/install_util.h',
            'util/l10n_string_util.cc',
            'util/l10n_string_util.h',
            'util/logging_installer.cc',
            'util/logging_installer.h',
            'util/lzma_util.cc',
            'util/lzma_util.h',
            'util/master_preferences.cc',
            'util/master_preferences.h',
            'util/move_tree_work_item.cc',
            'util/move_tree_work_item.h',
            'util/self_reg_work_item.cc',
            'util/self_reg_work_item.h',
            'util/set_reg_value_work_item.cc',
            'util/set_reg_value_work_item.h',
            'util/shell_util.cc',
            'util/shell_util.h',
            'util/util_constants.cc',
            'util/util_constants.h',
            'util/version.cc',
            'util/version.h',
            'util/work_item.cc',
            'util/work_item.h',
            'util/work_item_list.cc',
            'util/work_item_list.h',
          ],
        },
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
                '../../chrome_frame/chrome_frame.gyp:npchrome_tab',
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
      # Always google_chrome since this only applies to branding==Chrome.
      'variables': {
        'branding_dir': '../app/theme/google_chrome',
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
          'linux/internal/debian/postinst',
          'linux/internal/debian/postrm',
          'linux/internal/debian/prerm',
        ],
        'packaging_files_rpm': [
          'linux/internal/rpm/build.sh',
          'linux/internal/rpm/chrome.spec.template',
        ],
      },
      'targets': [
        {
          'target_name': 'installer_util',
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
          'target_name': 'linux_packages',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            '../chrome.gyp:chrome',
          ],
          'variables': {
            'version' : '<!(python <(version_py) -f ../../chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
            'revision' : '<!(python ../../build/util/lastchange.py | cut -d "=" -f 2)',
            'input_files': [
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
          'actions': [
            # TODO(mmoss) gyp looping construct would be handy here ...
            # These deb_packages* and rpm_packages* actions are the same except
            # for the 'channel' variable.
            {
              'variables': {
                'channel': 'unstable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(input_files)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(input_files)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
            {
              'variables': {
                'channel': 'stable',
              },
              'action_name': 'deb_packages_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(input_files)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/google-chrome-<(channel)_<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
          'conditions': [
            ['chromeos==0 and toolkit_views==0', {
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
                    '<@(input_files)',
                    '<@(packaging_files_common)',
                    '<@(packaging_files_rpm)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
                  ],
                  'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
                },
                {
                  'variables': {
                    'channel': 'beta',
                  },
                  'action_name': 'rpm_packages_<(channel)',
                  'process_outputs_as_sources': 1,
                  'inputs': [
                    '<(rpm_build)',
                    '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                    '<@(input_files)',
                    '<@(packaging_files_common)',
                    '<@(packaging_files_rpm)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
                  ],
                  'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
                },
                {
                  'variables': {
                    'channel': 'stable',
                  },
                  'action_name': 'rpm_packages_<(channel)',
                  'process_outputs_as_sources': 1,
                  'inputs': [
                    '<(rpm_build)',
                    '<(PRODUCT_DIR)/installer/rpm/chrome.spec.template',
                    '<@(input_files)',
                    '<@(packaging_files_common)',
                    '<@(packaging_files_rpm)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/google-chrome-<(channel)-<(version)-<(revision).<(rpm_arch).rpm',
                  ],
                  'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
                },
              ],
            }],
            ['target_arch=="x64"', {
              # TODO(mmoss) 64-bit RPMs not ready yet.
            }],
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
