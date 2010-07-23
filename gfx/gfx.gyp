# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gfx_unittests',
      'type': 'executable',
      'msvs_guid': 'C8BD2821-EAE5-4AC6-A0E4-F82CAC2956CC',
      'dependencies': [
        'gfx',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'blit_unittest.cc',
        'codec/jpeg_codec_unittest.cc',
        'codec/png_codec_unittest.cc',
        'color_utils_unittest.cc',
        'font_unittest.cc',
        'insets_unittest.cc',
        'rect_unittest.cc',
        'run_all_unittests.cc',
        'skbitmap_operations_unittest.cc',
        'test_suite.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'canvas_direct2d_unittest.cc',
            'icon_util_unittest.cc',
            'native_theme_win_unittest.cc',
          ],
          'include_dirs': [
            '..',
            '<(DEPTH)/third_party/wtl/include',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'd2d1.lib',
                'd3d10_1.lib',
              ],
            },
          }
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'gfx',
      'type': '<(library)',
      'msvs_guid': '13A8D36C-0467-4B4E-BAA3-FD69C45F076A',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjpeg/libjpeg.gyp:libjpeg',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        'blit.cc',
        'blit.h',
        'brush.h',
        'canvas.h',
        'canvas_skia.h',
        'canvas_skia.cc',
        'canvas_skia_linux.cc',
        'canvas_skia_mac.mm',
        'canvas_skia_paint.h',
        'canvas_skia_win.cc',
        'codec/jpeg_codec.cc',
        'codec/jpeg_codec.h',
        'codec/png_codec.cc',
        'codec/png_codec.h',
        'color_utils.cc',
        'color_utils.h',
        'favicon_size.h',
        'font.h',
        'font_gtk.cc',
        'font_mac.mm',
        'font_win.cc',
        'gfx_paths.cc',
        'gfx_paths.h',
        'insets.cc',
        'insets.h',
        'native_widget_types.h',
        'path.cc',
        'path.h',
        'path_gtk.cc',
        'path_win.cc',
        'point.cc',
        'point.h',
        'rect.cc',
        'rect.h',
        'scrollbar_size.cc',
        'scrollbar_size.h',
        'size.cc',
        'size.h',
        'skbitmap_operations.cc',
        'skbitmap_operations.h',
        'skia_util.cc',
        'skia_util.h',
        'skia_utils_gtk.cc',
        'skia_utils_gtk.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'canvas_direct2d.cc',
            'canvas_direct2d.h',
            'gdi_util.cc',
            'gdi_util.h',
            'icon_util.cc',
            'icon_util.h',
            'native_theme_win.cc',
            'native_theme_win.h',
            'window_impl.cc',
            'window_impl.h'
          ],
          'include_dirs': [
            '..',
            '<(DEPTH)/third_party/wtl/include',
          ],          
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'font_skia.cc',
            'gtk_native_view_id_manager.cc',
            'gtk_native_view_id_manager.h',
            'gtk_util.cc',
            'gtk_util.h',
            'native_widget_types_gtk.cc',
          ],
        }],
      ],
    },    
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
