# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
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
        '../ui/base/l10n/l10n_util_mac_unittest.mm',
        '../ui/base/l10n/l10n_util_unittest.cc',
        '../ui/base/models/tree_node_iterator_unittest.cc',
        '../ui/base/models/tree_node_model_unittest.cc',
        '../ui/base/resource/data_pack_unittest.cc',
        '../ui/base/resource/resource_bundle_unittest.cc',
        '../ui/base/test/data/resource.h',
        '../ui/base/text/text_elider_unittest.cc',
        '../ui/base/view_prop_unittest.cc',
        'run_all_unittests.cc',
        'sql/connection_unittest.cc',
        'sql/sqlite_features_unittest.cc',
        'sql/statement_unittest.cc',
        'sql/transaction_unittest.cc',
        'test_suite.cc',
        'test_suite.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'sources': [
            '../ui/base/dragdrop/gtk_dnd_util_unittest.cc',
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
            '../ui/base/strings/ui_strings.gyp:ui_unittest_strings',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            '../ui/base/dragdrop/os_exchange_data_win_unittest.cc',
            '../ui/base/view_prop_unittest.cc',
          ],
        }],
        ['os_posix==1 and OS!="mac"', {
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
      'target_name': 'app_resources',
      'type': 'none',
      'msvs_guid': '3FBC4235-3FBD-46DF-AEDC-BADBBA13A095',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app/app_resources',
      },
      'actions': [
        {
          'action_name': 'app_resources',
          'variables': {
            'grit_grd_file': 'resources/app_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
