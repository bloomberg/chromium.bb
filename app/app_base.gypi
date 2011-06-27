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
