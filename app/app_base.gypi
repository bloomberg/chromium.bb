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
            '../ui/base/system_monitor/system_monitor.cc',
            '../ui/base/system_monitor/system_monitor.h',
            '../ui/base/system_monitor/system_monitor_mac.mm',
            '../ui/base/system_monitor/system_monitor_posix.cc',
            '../ui/base/system_monitor/system_monitor_win.cc',
            'app_paths.h',
            'app_paths.cc',
            'app_switches.h',
            'app_switches.cc',
        ],
        'conditions': [
          ['OS!="linux" and OS!="freebsd" and OS!="openbsd"', {
            'sources!': [
              '../ui/base/dragdrop/gtk_dnd_util.cc',
              '../ui/base/dragdrop/gtk_dnd_util.h',
              '../ui/base/gtk/gtk_signal.h',
              '../ui/base/gtk/gtk_signal_registrar.cc',
              '../ui/base/gtk/gtk_signal_registrar.h',
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
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
        '<(libjpeg_gyp_path):libjpeg',
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
        '../ui/base/gtk/event_synthesis_gtk.cc',
        '../ui/base/gtk/event_synthesis_gtk.h',
        '../ui/base/gtk/gtk_signal.h',
        '../ui/base/gtk/gtk_signal_registrar.cc',
        '../ui/base/gtk/gtk_signal_registrar.h',
        '../ui/base/keycodes/keyboard_code_conversion.cc',
        '../ui/base/keycodes/keyboard_code_conversion.h',
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
        '../ui/base/message_box_flags.h',
        '../ui/base/models/accelerator.h',
        '../ui/base/models/accelerator_gtk.h',
        '../ui/base/models/accelerator_cocoa.h',
        '../ui/base/models/button_menu_item_model.cc',
        '../ui/base/models/button_menu_item_model.h',
        '../ui/base/models/menu_model.cc',
        '../ui/base/models/menu_model.h',
        '../ui/base/models/simple_menu_model.cc',
        '../ui/base/models/simple_menu_model.h',
        '../ui/base/models/combobox_model.h',
        '../ui/base/models/table_model.cc',
        '../ui/base/models/table_model.h',
        '../ui/base/models/table_model_observer.h',
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
        'data_pack.cc',
        'data_pack.h',
        'gfx/gl/gl_bindings.h',
        'gfx/gl/gl_context.cc',
        'gfx/gl/gl_context.h',
        'gfx/gl/gl_context_linux.cc',
        'gfx/gl/gl_context_mac.cc',
        'gfx/gl/gl_context_osmesa.cc',
        'gfx/gl/gl_context_osmesa.h',
        'gfx/gl/gl_context_stub.cc',
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
        'l10n_util.cc',
        'l10n_util.h',
        'l10n_util_collator.h',
        'l10n_util_mac.h',
        'l10n_util_mac.mm',
        'l10n_util_posix.cc',
        'l10n_util_win.cc',
        'l10n_util_win.h',
        'mac/nsimage_cache.h',
        'mac/nsimage_cache.mm',
        'mac/scoped_nsdisable_screen_updates.h',
        'resource_bundle.cc',
        'resource_bundle.h',
        'resource_bundle_linux.cc',
        'resource_bundle_mac.mm',
        'resource_bundle_posix.cc',
        'resource_bundle_win.cc',
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
        'win/iat_patch_function.cc',
        'win/iat_patch_function.h',
        'win/scoped_co_mem.h',
        'win/scoped_com_initializer.h',
        'win/scoped_prop.cc',
        'win/scoped_prop.h',
        'win/shell.cc',
        'win/shell.h',
        'win/win_util.cc',
        'win/win_util.h',
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
            'view_prop.cc',
            'view_prop.h',
            'win/iat_patch_function.cc',
            'win/iat_patch_function.h',
          ],
          'sources/': [
            ['exclude', '^win/*'],
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
          'sources!': [
            '../ui/base/keycodes/keyboard_code_conversion_mac.mm',
            '../ui/base/keycodes/keyboard_code_conversion_mac.h',
            '../ui/base/keycodes/keyboard_codes_win.h',
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
