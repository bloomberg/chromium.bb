# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'app_base',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/ui.gyp:ui_gfx',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
      ],
      'sources': [
        'app_paths.h',
        'app_paths.cc',
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
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'app_base_nacl_win64',
          'type': 'static_library',
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'sources': [
            '../ui/base/resource/resource_bundle_dummy.cc',
            '../ui/base/ui_base_paths.h',
            '../ui/base/ui_base_paths.cc',
            '../ui/base/ui_base_switches.h',
            '../ui/base/ui_base_switches.cc',
            'app_paths.h',
            'app_paths.cc',
          ],
          'include_dirs': [
            '..',
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
