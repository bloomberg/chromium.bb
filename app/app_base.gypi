# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
            'app_paths.h',
            'app_paths.cc',
            'app_switches.h',
            'app_switches.cc',
            'hi_res_timer_manager_posix.cc',
            'hi_res_timer_manager_win.cc',
            'hi_res_timer_manager.h',
            'system_monitor.cc',
            'system_monitor.h',
            'system_monitor_posix.cc',
            'system_monitor_win.cc',
            'tree_model.h',
            'tree_node_iterator.h',
            'tree_node_model.h',
        ],
        'conditions': [
          ['OS=="win"', {
            'sources': [
              'win_util.cc',
              'win_util.h',
              'win_util_path.cc',
            ],
          }],
          ['OS!="linux" and OS!="freebsd" and OS!="openbsd"', {
            'sources!': [
              'gtk_dnd_util.cc',
              'gtk_dnd_util.h',
              'gtk_signal.h',
              'gtk_signal_registrar.cc',
              'gtk_signal_registrar.h',
              'gtk_util.cc',
              'gtk_util.h',
              'x11_util.cc',
              'x11_util.h',
              'x11_util_internal.h',
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
        'gl_binding_output_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
      },
      'dependencies': [
        # app resources and app_strings should be shared with the 64-bit
        # target, but it doesn't work due to a bug in gyp
        'app_resources',
        'app_strings',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../gfx/gfx.gyp:gfx',
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
        '../third_party/mesa/MesaLib/include',
        '<(gl_binding_output_dir)',
      ],
      # TODO(gregoryd): The direct_dependent_settings should be shared with
      # the 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
          '../third_party/mesa/MesaLib/include',
          '<(gl_binding_output_dir)',
        ],
      },
      'sources': [
        # Files that are not required for Win64 Native Client loader
        'active_window_watcher_x.cc',
        'active_window_watcher_x.h',
        'animation_container.cc',
        'animation_container.h',
        'animation.cc',
        'animation.h',
        'bidi_line_iterator.cc',
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
        'file_download_interface.h',
        'gfx/font_util.h',
        'gfx/font_util.cc',
        'gfx/gl/gl_bindings.h',
        'gfx/gl/gl_context.cc',
        'gfx/gl/gl_context.h',
        'gfx/gl/gl_context_linux.cc',
        'gfx/gl/gl_context_mac.cc',
        'gfx/gl/gl_context_osmesa.cc',
        'gfx/gl/gl_context_osmesa.h',
        'gfx/gl/gl_context_stub.h',
        'gfx/gl/gl_context_win.cc',
        'gfx/gl/gl_headers.h',
        'gfx/gl/gl_implementation.cc',
        'gfx/gl/gl_implementation.h',
        'gfx/gl/gl_implementation_linux.cc',
        'gfx/gl/gl_implementation_mac.cc',
        'gfx/gl/gl_implementation_win.cc',
        'gfx/gl/gl_interface.h',
        'gfx/gl/gl_interface.cc',
        'gfx/gl/gl_mock.h',
        'gtk_dnd_util.cc',
        'gtk_dnd_util.h',
        'gtk_signal.h',
        'gtk_signal_registrar.cc',
        'gtk_signal_registrar.h',
        'gtk_util.cc',
        'gtk_util.h',
        'l10n_util.cc',
        'l10n_util.h',
        'l10n_util_mac.h',
        'l10n_util_mac.mm',
        'l10n_util_posix.cc',
        'l10n_util_win.cc',
        'l10n_util_win.h',
        'linear_animation.cc',
        'linear_animation.h',
        'menus/accelerator.h',
        'menus/accelerator_gtk.h',
        'menus/accelerator_cocoa.h',
        'menus/button_menu_item_model.cc',
        'menus/button_menu_item_model.h',
        'menus/menu_model.cc',
        'menus/menu_model.h',
        'menus/simple_menu_model.cc',
        'menus/simple_menu_model.h',
        'message_box_flags.h',
        'multi_animation.cc',
        'multi_animation.h',
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
        'sql/diagnostic_error_delegate.h',
        'sql/init_status.h',
        'sql/meta_table.cc',
        'sql/meta_table.h',
        'sql/statement.cc',
        'sql/statement.h',
        'sql/transaction.cc',
        'sql/transaction.h',
        'surface/accelerated_surface_mac.cc',
        'surface/accelerated_surface_mac.h',
        'surface/io_surface_support_mac.cc',
        'surface/io_surface_support_mac.h',
        'surface/transport_dib.h',
        'surface/transport_dib_linux.cc',
        'surface/transport_dib_mac.cc',
        'surface/transport_dib_win.cc',
        'table_model.cc',
        'table_model.h',
        'table_model_observer.h',
        'text_elider.cc',
        'text_elider.h',
        'theme_provider.cc',
        'theme_provider.h',
        'throb_animation.cc',
        'throb_animation.h',
        'tween.cc',
        'tween.h',
        'x11_util.cc',
        'x11_util.h',
        'x11_util_internal.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
      ],
      # hard_dependency is necessary for this target because it has actions
      # that generate header files included by dependent targtets. The header
      # files must be generated before the dependents are compiled. The usual
      # semantics are to allow the two targets to build concurrently.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_gl_bindings',
          'inputs': [
            'gfx/gl/generate_bindings.py',
          ],
          'outputs': [
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
          ],
          'action': [
            'python',
            'gfx/gl/generate_bindings.py',
            '<(gl_binding_output_dir)',
          ],
        },
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
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
                ['exclude', '^os_exchange_data.cc'],
                ['exclude', '^os_exchange_data.h'],
                ['exclude', '^os_exchange_data_provider_gtk.cc'],
                ['exclude', '^os_exchange_data_provider_gtk.h'],
                ['exclude', '^drag_drop_types_gtk.cc'],
              ],
            }],
            ['toolkit_views==1', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['include', '^os_exchange_data.cc'],
              ],
            }],
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
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'gfx/gl/gl_context_egl.cc',
            'gfx/gl/gl_context_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
          ],
          'include_dirs': [
            # We don't use angle, but pull the EGL/GLES headers from there.
            '../third_party/angle/include',
          ],
          'all_dependent_settings': {
            'defines': [
              'GL_GLEXT_PROTOTYPES',
            ],
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
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/angle/include',
          ],
          'sources': [
            'gfx/gl/gl_context_egl.cc',
            'gfx/gl/gl_context_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
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
            # app resources and app_strings should be shared with the 32-bit
            # target, but it doesn't work due to a bug in gyp
            'app_resources',
            'app_strings',
            '../base/base.gyp:base_nacl_win64',
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
            'l10n_util_dummy.cc',
            'resource_bundle_dummy.cc',
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
