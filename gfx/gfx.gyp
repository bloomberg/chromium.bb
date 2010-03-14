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
        'gfx_paths.cc',
        'gfx_paths.h',
        'point.cc',
        'point.h',
        'rect.cc',
        'rect.h',
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
      ],
    },    
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
