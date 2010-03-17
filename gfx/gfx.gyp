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
        'insets_unittest.cc',
        'rect_unittest.cc',
        'run_all_unittests.cc',
        'test_suite.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'icon_util_unittest.cc',
          ],
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
        'gfx_paths.cc',
        'gfx_paths.h',
        'insets.cc',
        'insets.h',
        'native_widget_types.h',
        'native_widget_types_gtk.cc',
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
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'icon_util.cc',
            'icon_util.h',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources': [
            'gtk_native_view_id_manager.cc',
            'gtk_native_view_id_manager.h',
            'gtk_util.cc',
            'gtk_util.h',
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
