{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'alternate_version_generator_lib',
          'msvs_guid': '66723D46-A641-4182-A321-923FD6335D9B',
          'type': 'static_library',
          'dependencies': [
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/alternate_version_generator.cc',
            'test/alternate_version_generator.h',
            'test/pe_image_resources.cc',
            'test/pe_image_resources.h',
            'test/resource_loader.cc',
            'test/resource_loader.h',
            'test/resource_updater.cc',
            'test/resource_updater.h',
          ],
        },
        {
          'target_name': 'upgrade_test',
          'msvs_guid': 'BC4D6130-FDAD-47FB-B4FD-FCAF78DCBC3C',
          'type': 'executable',
          'dependencies': [
            'alternate_version_generator_lib',
            # This dependency, although correct, results in the mini installer
            # being rebuilt every time upgrade_test is built.  So disable it
            # for now.
            # TODO(grt): fix rules/targets/etc for
            # mini_installer.gyp:mini_installer so that it does no work if
            # nothing has changed, then un-comment this next line:
            # 'mini_installer.gyp:mini_installer',
            '../../base/base.gyp:test_support_base',
            '../../testing/gtest.gyp:gtest',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/run_all_tests.cc',
            'test/upgrade_test.cc',
          ],
        },
        {
          'target_name': 'alternate_version_generator',
          'msvs_guid': 'E6E6B339-AEC0-44C9-B9D0-E30138108379',
          'type': 'executable',
          'dependencies': [
            'alternate_version_generator_lib',
            '../../base/base.gyp:test_support_base',
            '../../testing/gtest.gyp:gtest',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/alternate_version_generator_main.cc',
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
