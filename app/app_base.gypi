# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'app_base_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below. Only files and
      # settings relevant for building the Win64 target should be added here.
      # All the rest should be added to the 'app_base' target below.
      ['app_base_target==1', {
        'sources': [
            # Used both for Chrome and for Win64 NaCl loader
            '../ui/base/models/tree_model.cc',
            '../ui/base/models/tree_model.h',
            '../ui/base/models/tree_node_iterator.h',
            '../ui/base/models/tree_node_model.h',
            '../ui/base/ui_base_paths.h',
            '../ui/base/ui_base_paths.cc',
            '../ui/base/ui_base_switches.h',
            '../ui/base/ui_base_switches.cc',
            'app_paths.h',
            'app_paths.cc',
        ],
        'conditions': [
          ['toolkit_uses_gtk!=1', {
            'sources!': [
              '../ui/base/dragdrop/gtk_dnd_util.cc',
              '../ui/base/dragdrop/gtk_dnd_util.h',
              '../ui/base/gtk/gtk_signal.h',
              '../ui/base/gtk/gtk_signal_registrar.cc',
              '../ui/base/gtk/gtk_signal_registrar.h',
              '../ui/base/gtk/gtk_windowing.cc',
              '../ui/base/gtk/gtk_windowing.h',
              '../ui/base/x/x11_util.cc',
              '../ui/base/x/x11_util.h',
              '../ui/base/x/x11_util_internal.h',
            ],
          }],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'app_base',
      'type': '<(library)',
      'msvs_guid': '4631946D-7D5F-44BD-A5A8-504C0A7033BE',
      'variables': {
        'app_base_target': 1,
      },
      'dependencies': [
        # app resources and ui_strings should be shared with the 64-bit
        # target, but it doesn't work due to a bug in gyp
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:base_static',
        '../ui/ui.gyp:ui_gfx',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/base/strings/ui_strings.gyp:ui_strings',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
      ],
      # TODO(gregoryd): The direct_dependent_settings should be shared with
      # the 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        # Files that are not required for Win64 Native Client loader
        '../ui/base/animation/animation.cc',
        '../ui/base/animation/animation.h',
        '../ui/base/animation/animation_container.cc',
        '../ui/base/animation/animation_container.h',
        '../ui/base/animation/animation_container_element.h',
        '../ui/base/animation/animation_container_observer.h',
        '../ui/base/animation/animation_delegate.h',
        '../ui/base/animation/linear_animation.cc',
        '../ui/base/animation/linear_animation.h',
        '../ui/base/animation/multi_animation.cc',
        '../ui/base/animation/multi_animation.h',
        '../ui/base/animation/slide_animation.cc',
        '../ui/base/animation/slide_animation.h',
        '../ui/base/animation/throb_animation.cc',
        '../ui/base/animation/throb_animation.h',
        '../ui/base/animation/tween.cc',
        '../ui/base/animation/tween.h',
        '../ui/base/clipboard/clipboard.cc',
        '../ui/base/clipboard/clipboard.h',
        '../ui/base/clipboard/clipboard_linux.cc',
        '../ui/base/clipboard/clipboard_mac.mm',
        '../ui/base/clipboard/clipboard_util_win.cc',
        '../ui/base/clipboard/clipboard_util_win.h',
        '../ui/base/clipboard/clipboard_win.cc',
        '../ui/base/clipboard/scoped_clipboard_writer.cc',
        '../ui/base/clipboard/scoped_clipboard_writer.h',
        '../ui/base/dragdrop/drag_drop_types_gtk.cc',
        '../ui/base/dragdrop/drag_drop_types_win.cc',
        '../ui/base/dragdrop/drag_drop_types.h',
        '../ui/base/dragdrop/drag_source.cc',
        '../ui/base/dragdrop/drag_source.h',
        '../ui/base/dragdrop/drop_target.cc',
        '../ui/base/dragdrop/drop_target.h',
        '../ui/base/dragdrop/gtk_dnd_util.cc',
        '../ui/base/dragdrop/gtk_dnd_util.h',
        '../ui/base/dragdrop/os_exchange_data_provider_gtk.cc',
        '../ui/base/dragdrop/os_exchange_data_provider_gtk.h',
        '../ui/base/dragdrop/os_exchange_data_provider_win.cc',
        '../ui/base/dragdrop/os_exchange_data_provider_win.h',
        '../ui/base/dragdrop/os_exchange_data.cc',
        '../ui/base/dragdrop/os_exchange_data.h',
        '../ui/base/events.h',
        '../ui/base/gtk/event_synthesis_gtk.cc',
        '../ui/base/gtk/event_synthesis_gtk.h',
        '../ui/base/gtk/gtk_signal.h',
        '../ui/base/gtk/gtk_signal_registrar.cc',
        '../ui/base/gtk/gtk_signal_registrar.h',
        '../ui/base/gtk/gtk_windowing.cc',
        '../ui/base/gtk/gtk_windowing.h',
        '../ui/base/keycodes/keyboard_code_conversion_gtk.cc',
        '../ui/base/keycodes/keyboard_code_conversion_gtk.h',
        '../ui/base/keycodes/keyboard_code_conversion_mac.mm',
        '../ui/base/keycodes/keyboard_code_conversion_mac.h',
        '../ui/base/keycodes/keyboard_code_conversion_win.cc',
        '../ui/base/keycodes/keyboard_code_conversion_win.h',
        '../ui/base/keycodes/keyboard_code_conversion_x.cc',
        '../ui/base/keycodes/keyboard_code_conversion_x.h',
        '../ui/base/keycodes/keyboard_codes.h',
        '../ui/base/keycodes/keyboard_codes_win.h',
        '../ui/base/keycodes/keyboard_codes_posix.h',
        '../ui/base/l10n/l10n_font_util.h',
        '../ui/base/l10n/l10n_font_util.cc',
        '../ui/base/l10n/l10n_util.cc',
        '../ui/base/l10n/l10n_util.h',
        '../ui/base/l10n/l10n_util_collator.h',
        '../ui/base/l10n/l10n_util_mac.h',
        '../ui/base/l10n/l10n_util_mac.mm',
        '../ui/base/l10n/l10n_util_posix.cc',
        '../ui/base/l10n/l10n_util_win.cc',
        '../ui/base/l10n/l10n_util_win.h',
        '../ui/base/message_box_flags.h',
        '../ui/base/message_box_win.cc',
        '../ui/base/message_box_win.h',
        '../ui/base/models/accelerator.h',
        '../ui/base/models/accelerator_gtk.h',
        '../ui/base/models/accelerator_cocoa.mm',
        '../ui/base/models/accelerator_cocoa.h',
        '../ui/base/models/button_menu_item_model.cc',
        '../ui/base/models/button_menu_item_model.h',
        '../ui/base/models/menu_model.cc',
        '../ui/base/models/menu_model.h',
        '../ui/base/models/menu_model_delegate.h',
        '../ui/base/models/simple_menu_model.cc',
        '../ui/base/models/simple_menu_model.h',
        '../ui/base/models/combobox_model.h',
        '../ui/base/models/table_model.cc',
        '../ui/base/models/table_model.h',
        '../ui/base/models/table_model_observer.h',
        '../ui/base/resource/data_pack.cc',
        '../ui/base/resource/data_pack.h',
        '../ui/base/resource/resource_bundle.cc',
        '../ui/base/resource/resource_bundle.h',
        '../ui/base/resource/resource_bundle_linux.cc',
        '../ui/base/resource/resource_bundle_mac.mm',
        '../ui/base/resource/resource_bundle_posix.cc',
        '../ui/base/resource/resource_bundle_win.cc',
        '../ui/base/text/text_elider.cc',
        '../ui/base/text/text_elider.h',
        '../ui/base/theme_provider.cc',
        '../ui/base/theme_provider.h',
        '../ui/base/view_prop.cc',
        '../ui/base/view_prop.h',
        '../ui/base/win/hwnd_util.cc',
        '../ui/base/win/hwnd_util.h',
        '../ui/base/win/window_impl.cc',
        '../ui/base/win/window_impl.h',
        '../ui/base/x/active_window_watcher_x.cc',
        '../ui/base/x/active_window_watcher_x.h',
        '../ui/base/x/x11_util.cc',
        '../ui/base/x/x11_util.h',
        '../ui/base/x/x11_util_internal.h',
        'mac/nsimage_cache.h',
        'mac/nsimage_cache.mm',
        'mac/scoped_nsdisable_screen_updates.h',
        'sql/connection.cc',
        'sql/connection.h',
        'sql/diagnostic_error_delegate.h',
        'sql/init_status.h',
        'sql/meta_table.cc',
        'sql/meta_table.h',
        'sql/statement.cc',
        'sql/statement.h',
        'sql/transaction.cc',
        'sql/transaction.h',
        'win/iat_patch_function.cc',
        'win/iat_patch_function.h',
        'win/scoped_co_mem.h',
        'win/scoped_com_initializer.h',
        'win/scoped_prop.cc',
        'win/scoped_prop.h',
        'win/shell.cc',
        'win/shell.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:xext',
          ],
          'link_settings': {
            'libraries': [
              '-lXrender',  # For XRender* function calls in x11_util.cc.
            ],
          },
          'conditions': [
            ['toolkit_views==0', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['exclude', '^../ui/base/dragdrop/drag_drop_types_gtk.cc'],
                ['exclude', '^../ui/base/dragdrop/os_exchange_data.cc'],
                ['exclude', '^../ui/base/dragdrop/os_exchange_data.h'],
                ['exclude', '^../ui/base/dragdrop/os_exchange_data_provider_gtk.cc'],
                ['exclude', '^../ui/base/dragdrop/os_exchange_data_provider_gtk.h'],
              ],
            }],
            ['toolkit_views==1', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['include', '^../ui/base/dragdrop/os_exchange_data.cc'],
              ],
            }],
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            '../ui/base/dragdrop/drag_source.cc',
            '../ui/base/dragdrop/drag_source.h',
            '../ui/base/dragdrop/drag_drop_types.h',
            '../ui/base/dragdrop/drop_target.cc',
            '../ui/base/dragdrop/drop_target.h',
            '../ui/base/dragdrop/os_exchange_data.cc',
            '../ui/base/view_prop.cc',
            '../ui/base/view_prop.h',
            'win/iat_patch_function.cc',
            'win/iat_patch_function.h',
          ],
          'sources/': [
            ['exclude', '^win/*'],
          ],
        }],
        ['use_x11==1', {
          'sources!': [
            '../ui/base/keycodes/keyboard_code_conversion_mac.mm',
            '../ui/base/keycodes/keyboard_code_conversion_mac.h',
            '../ui/base/keycodes/keyboard_codes_win.h',
          ],
          'all_dependent_settings': {
            'ldflags': [
              '-L<(PRODUCT_DIR)',
            ],
            'link_settings': {
              'libraries': [
                '-lX11',
                '-ldl',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
              '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
            ],
          },
          'sources!': [
            '../ui/base/keycodes/keyboard_code_conversion_gtk.cc',
            '../ui/base/keycodes/keyboard_code_conversion_gtk.h',
            '../ui/base/keycodes/keyboard_code_conversion_x.cc',
            '../ui/base/keycodes/keyboard_code_conversion_x.h',
            '../ui/base/keycodes/keyboard_codes_win.h',
            '../ui/base/gtk/event_synthesis_gtk.cc',
            '../ui/base/gtk/event_synthesis_gtk.h',
          ],
        }],
        ['OS=="win"', {
          'sources!': [
            '../ui/base/keycodes/keyboard_code_conversion_gtk.cc',
            '../ui/base/keycodes/keyboard_code_conversion_gtk.h',
            '../ui/base/keycodes/keyboard_code_conversion_mac.mm',
            '../ui/base/keycodes/keyboard_code_conversion_mac.h',
            '../ui/base/keycodes/keyboard_code_conversion_x.cc',
            '../ui/base/keycodes/keyboard_code_conversion_x.h',
            '../ui/base/keycodes/keyboard_codes_posix.h',
            '../ui/base/gtk/event_synthesis_gtk.cc',
            '../ui/base/gtk/event_synthesis_gtk.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'app_base_nacl_win64',
          'type': '<(library)',
          'msvs_guid': '4987C6F9-B230-48E5-BF91-418EAE69AD90',
          'dependencies': [
            # app resources and ui_strings should be shared with the 32-bit
            # target, but it doesn't work due to a bug in gyp
            'app_resources',
            '../base/base.gyp:base_nacl_win64',
            '../ui/base/strings/ui_strings.gyp:ui_strings',
          ],
          'variables': {
            'app_base_target': 1,
          },
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          # TODO(gregoryd): The direct_dependent_settings should be shared with
          # the 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'sources': [
            '../ui/base/resource/resource_bundle_dummy.cc',
          ],
          'include_dirs': [
            '../skia/config/win',
            '../third_party/icu/public/common',
            '../third_party/icu/public/i18n',
            '../third_party/npapi',
            '../third_party/skia/include/config',
            '../third_party/skia/include/core',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
