{
  'variables': {
    'version_py': '<(DEPTH)/chrome/tools/build/version.py',
    'version_path': '<(DEPTH)/chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'validate_installation',
          'msvs_guid': '7CC08DA8-E9CA-4573-A8C4-E7F0D0CF8EBA',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:common_constants',
            '<(DEPTH)/chrome/chrome.gyp:installer_util',
            '<(DEPTH)/chrome/chrome.gyp:installer_util_strings',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'tools/validate_installation_main.cc',
            'tools/validate_installation.rc',
            'tools/validate_installation_resource.h',
          ],
        },
      ],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '<(DEPTH)/chrome/app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '<(DEPTH)/chrome/app/theme/chromium',
      },
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
