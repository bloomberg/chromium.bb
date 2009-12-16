# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '../chrome/tools/build/apply_locales.py',],
    'chromium_code': 1,
    'grit_info_cmd': ['python', '../tools/grit/grit_info.py',],
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
    'grit_cmd': ['python', '../tools/grit/grit.py'],
    'localizable_resources': [
      'resources/app_locale_settings.grd',
      'resources/app_strings.grd',
    ],
  },
  'includes': [
    'app_base.gypi',
  ],
  'targets': [
    {
      'target_name': 'app_unittests',
      'type': 'executable',
      'msvs_guid': 'B4D59AE8-8D2F-97E1-A8E9-6D2826729530',
      'dependencies': [
        'app_base',
        'app_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjpeg/libjpeg.gyp:libjpeg',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        'animation_unittest.cc',
        'clipboard/clipboard_unittest.cc',
        'gfx/codec/jpeg_codec_unittest.cc',
        'gfx/codec/png_codec_unittest.cc',
        'gfx/color_utils_unittest.cc',
        'gfx/font_unittest.cc',
        'gfx/icon_util_unittest.cc',
        'gfx/insets_unittest.cc',
        'gfx/native_theme_win_unittest.cc',
        'gfx/skbitmap_operations_unittest.cc',
        'gfx/text_elider_unittest.cc',
        'l10n_util_mac_unittest.mm',
        'l10n_util_unittest.cc',
        'os_exchange_data_win_unittest.cc',
        'run_all_unittests.cc',
        'system_monitor_unittest.cc',
        'test_suite.h',
        'sql/connection_unittest.cc',
        'sql/statement_unittest.cc',
        'sql/transaction_unittest.cc',
        'tree_node_iterator_unittest.cc',
        'win_util_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'gfx/icon_util_unittest.cc',
            'gfx/native_theme_win_unittest.cc',
            'os_exchange_data_win_unittest.cc',
            'win_util_unittest.cc',
          ],
        }],
        ['OS =="linux" or OS =="freebsd"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'app_strings',
      'msvs_guid': 'AE9BF4A2-19C5-49D8-BB1A-F28496DD7051',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(localizable_resources))',
          ],
          'outputs': [
            '<(grit_out_dir)/<(RULE_INPUT_ROOT)/grit/<(RULE_INPUT_ROOT).h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(RULE_INPUT_ROOT)/<(RULE_INPUT_ROOT)_ZZLOCALE.pak\' <(locales))',
          ],
          'action': ['<@(grit_cmd)', '-i', '<(RULE_INPUT_PATH)',
            'build', '-o', '<(grit_out_dir)/<(RULE_INPUT_ROOT)'],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
          'conditions': [
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
        },
      ],
      'sources': [
        '<@(localizable_resources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)/app_locale_settings',
          '<(grit_out_dir)/app_strings',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'app_resources',
      'type': 'none',
      'msvs_guid': '3FBC4235-3FBD-46DF-AEDC-BADBBA13A095',
      'actions': [
        {
          'action_name': 'app_resources',
          'variables': {
            'input_path': 'resources/app_resources.grd',
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)/app_resources\' <(input_path))',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(input_path)', 'build',
                     '-o', '<(grit_out_dir)/app_resources'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)/app_resources',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'app_id',
      'type': 'none',
      'msvs_guid': '83100055-172B-49EA-B422-B1A92B627D37',
      'conditions': [
        ['OS=="win"',
          {
            'direct_dependent_settings': {
              'include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome/app_id',
              ],
            },
            'actions': [
              {
                'action_name': 'appid',
                'variables': {
                  'appid_py': '../chrome/tools/build/appid.py',
                },
                'conditions': [
                  [ 'branding=="Chrome"', {
                    'variables': {
                      'appid_value': '<(google_update_appid)',
                    },
                  }, { # else
                    'variables': {
                      'appid_value': '',
                    },
                  }],
                ],
                'inputs': [
                  '<(appid_py)',
                ],
                'outputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/chrome/app_id/appid.h',
                  'tools/build/_always_run_appid_py.marker',
                ],
                'action': [
                  'python',
                  '<(appid_py)',
                  '-a', '<(appid_value)',
                  '-o', '<(SHARED_INTERMEDIATE_DIR)/chrome/app_id/appid.h',
                ],
                'process_outputs_as_sources': 1,
                'message': 'Generating appid information in <(SHARED_INTERMEDIATE_DIR)/chrome/appid.h'
              },
            ],
          },
        ],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
