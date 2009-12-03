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
  'targets': [
    {
      'target_name': 'app_base',
      'type': '<(library)',
      'msvs_guid': '4631946D-7D5F-44BD-A5A8-504C0A7033BE',
      'dependencies': [
        'app_resources',
        'app_strings',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjpeg/libjpeg.gyp:libjpeg',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
      ],
      'include_dirs': [
        '..',
        '../chrome/third_party/wtl/include',
      ],
      'sources': [
        # All .cc, .h, and .mm files under app/ except for tests.
        'animation.cc',
        'animation.h',
        'active_window_watcher_x.cc',
        'active_window_watcher_x.h',
        'app_paths.h',
        'app_paths.cc',
        'app_switches.h',
        'app_switches.cc',
        'clipboard/clipboard.cc',
        'clipboard/clipboard.h',
        'clipboard/clipboard_linux.cc',
        'clipboard/clipboard_mac.mm',
        'clipboard/clipboard_util_win.cc',
        'clipboard/clipboard_util_win.h',
        'clipboard/clipboard_win.cc',
        'clipboard/scoped_clipboard_writer.cc',
        'clipboard/scoped_clipboard_writer.h',
        'combobox_model.h',
        'drag_drop_types_gtk.cc',
        'drag_drop_types_win.cc',
        'drag_drop_types.h',
        'gfx/blit.cc',
        'gfx/blit.h',
        'gfx/canvas.cc',
        'gfx/canvas.h',
        'gfx/canvas_linux.cc',
        'gfx/canvas_mac.mm',
        'gfx/canvas_win.cc',
        'gfx/codec/jpeg_codec.cc',
        'gfx/codec/jpeg_codec.h',
        'gfx/codec/png_codec.cc',
        'gfx/codec/png_codec.h',
        'gfx/color_utils.cc',
        'gfx/color_utils.h',
        'gfx/favicon_size.h',
        'gfx/font.h',
        'gfx/font_gtk.cc',
        'gfx/font_mac.mm',
        'gfx/font_skia.cc',
        'gfx/font_util.h',
        'gfx/font_util.cc',
        'gfx/font_win.cc',
        'gfx/gdi_util.cc',
        'gfx/gdi_util.h',
        'gfx/gtk_util.cc',
        'gfx/gtk_util.h',
        'gfx/icon_util.cc',
        'gfx/icon_util.h',
        'gfx/insets.h',
        'gfx/native_widget_types.h',
        'gfx/native_widget_types_gtk.cc',
        'gfx/native_theme_win.cc',
        'gfx/native_theme_win.h',
        'gfx/gtk_native_view_id_manager.cc',
        'gfx/gtk_native_view_id_manager.h',
        'gfx/path.cc',
        'gfx/path_gtk.cc',
        'gfx/path_win.cc',
        'gfx/path.h',
        'gfx/skbitmap_operations.cc',
        'gfx/skbitmap_operations.h',
        'gfx/text_elider.cc',
        'gfx/text_elider.h',
        'gtk_dnd_util.cc',
        'gtk_dnd_util.h',
        'hi_res_timer_manager_posix.cc',
        'hi_res_timer_manager_win.cc',
        'hi_res_timer_manager.h',
        'l10n_util.cc',
        'l10n_util.h',
        'l10n_util_mac.h',
        'l10n_util_mac.mm',
        'l10n_util_posix.cc',
        'l10n_util_win.cc',
        'l10n_util_win.h',
        'message_box_flags.h',
        'os_exchange_data_provider_gtk.cc',
        'os_exchange_data_provider_gtk.h',
        'os_exchange_data_provider_win.cc',
        'os_exchange_data_provider_win.h',
        'os_exchange_data.cc',
        'os_exchange_data.h',
        'resource_bundle.cc',
        'resource_bundle.h',
        'resource_bundle_linux.cc',
        'resource_bundle_mac.mm',
        'resource_bundle_posix.cc',
        'resource_bundle_win.cc',
        'slide_animation.cc',
        'slide_animation.h',
        'sql/connection.cc',
        'sql/connection.h',
        'sql/meta_table.cc',
        'sql/meta_table.h',
        'sql/statement.cc',
        'sql/statement.h',
        'sql/transaction.cc',
        'sql/transaction.h',
        'system_monitor.cc',
        'system_monitor.h',
        'system_monitor_posix.cc',
        'system_monitor_win.cc',
        'table_model.cc',
        'table_model.h',
        'table_model_observer.h',
        'theme_provider.cc',
        'theme_provider.h',
        'throb_animation.cc',
        'throb_animation.h',
        'tree_model.h',
        'tree_node_iterator.h',
        'tree_node_model.h',
        'win/window_impl.cc',
        'win/window_impl.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
          ],
          'conditions': [
            ['toolkit_views==0 and chromeos==0', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['exclude', '^os_exchange_data.cc'],
                ['exclude', '^os_exchange_data.h'],
                ['exclude', '^os_exchange_data_provider_gtk.cc'],
                ['exclude', '^os_exchange_data_provider_gtk.h'],
                ['exclude', '^drag_drop_types_gtk.cc'],
              ],
            }],
            ['toolkit_views==1 or chromeos==1', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['include', '^os_exchange_data.cc'],
              ],
            }],
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win_util.cc',
            'win_util.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'drag_drop_types.h',
            'gfx/gdi_util.cc',
            'gfx/gdi_util.h',
            'gfx/icon_util.cc',
            'gfx/icon_util.h',
            'gfx/native_theme_win.cc',
            'gfx/native_theme_win.h',
            'os_exchange_data.cc',
            'win/window_impl.cc',
            'win/window_impl.h',
          ],
        }],
        ['OS!="linux"', {
          'sources!': [
            'gfx/gtk_util.cc',
            'gfx/gtk_util.h',
            'gtk_dnd_util.cc',
            'gtk_dnd_util.h',
          ],
        }],
      ],
    },
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
