{
  'variables': {
    'chromium_code': 1,
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
    'mini_installer_internal_deps%': 0,
    'mini_installer_official_deps%': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'mini_installer',
          'variables': {
            'chrome_dll_project': [
              '../chrome.gyp:chrome_dll',
            ],
            'chrome_dll_path': [
              '<(PRODUCT_DIR)/chrome.dll',
            ],
            'output_dir': '<(PRODUCT_DIR)',
          },
          'includes': [
            'mini_installer.gypi',
          ],
        }
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'mini_installer_tests_run',
              'type': 'none',
              'dependencies': [
                'mini_installer',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'mini_installer_tests.isolate',
              ],
            },
          ],
        }],
      ],
    }],
  ],
}
