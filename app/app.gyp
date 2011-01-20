# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '<(DEPTH)/build/apply_locales.py',],
    'chromium_code': 1,
    'grit_info_cmd': ['python', '../tools/grit/grit_info.py',
                      '<@(grit_defines)'],
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
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'sources': [
        '../ui/base/animation/animation_container_unittest.cc',
        '../ui/base/animation/animation_unittest.cc',
        '../ui/base/animation/multi_animation_unittest.cc',
        '../ui/base/animation/slide_animation_unittest.cc',
        '../ui/base/clipboard/clipboard_unittest.cc',
        '../ui/base/dragdrop/os_exchange_data_win_unittest.cc',
        '../ui/base/models/tree_node_iterator_unittest.cc',
        '../ui/base/models/tree_node_model_unittest.cc',
        '../ui/base/system_monitor/system_monitor_unittest.cc',
        '../ui/base/text/text_elider_unittest.cc',
        '../ui/base/view_prop_unittest.cc',
        'data_pack_unittest.cc',
        'l10n_util_mac_unittest.mm',
        'l10n_util_unittest.cc',
        'run_all_unittests.cc',
        'sql/connection_unittest.cc',
        'sql/statement_unittest.cc',
        'sql/transaction_unittest.cc',
        'test_suite.h',
        'test/data/resource.h',
        'win/win_util_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'sources': [
            '../ui/base/dragdrop/gtk_dnd_util_unittest.cc',
          ],
          'dependencies': [
            'app_unittest_strings',
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            '../ui/base/dragdrop/os_exchange_data_win_unittest.cc',
            '../ui/base/view_prop_unittest.cc',
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
            'build', '-o', '<(grit_out_dir)/<(RULE_INPUT_ROOT)',
            '<@(grit_defines)'],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
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
                     '-o', '<(grit_out_dir)/app_resources',
                     '<@(grit_defines)' ],
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
  ],
  'conditions': [
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
      'targets': [{
        'target_name': 'app_unittest_strings',
        'type': 'none',
        'variables': {
          'repack_path': '<(DEPTH)/tools/data_pack/repack.py',
        },
        'actions': [
          {
            'action_name': 'repack_app_unittest_strings',
            'variables': {
              'pak_inputs': [
                '<(grit_out_dir)/app_strings/app_strings_en-US.pak',
                '<(grit_out_dir)/app_locale_settings/app_locale_settings_en-US.pak',
              ],
            },
            'inputs': [
              '<(repack_path)',
              '<@(pak_inputs)',
            ],
            'outputs': [
              '<(PRODUCT_DIR)/app_unittests_strings/en-US.pak',
            ],
            'action': ['python', '<(repack_path)', '<@(_outputs)',
                       '<@(pak_inputs)'],
          },
        ],
        'copies': [
          {
            'destination': '<(PRODUCT_DIR)/app_unittests_strings',
            'files': [
              '<(grit_out_dir)/app_resources/app_resources.pak',
            ],
          },
        ],
      }],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
