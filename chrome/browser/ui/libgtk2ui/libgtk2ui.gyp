# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gtk2ui',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../base/base.gyp:base_i18n',
        '../../../../build/linux/system.gyp:gtk',
        '../../../../skia/skia.gyp:skia',
        '../../../../ui/base/strings/ui_strings.gyp:ui_strings',
        '../../../../ui/ui.gyp:ui',
        '../../../../ui/ui.gyp:ui_resources',
        '../../../../ui/linux_ui/linux_ui.gyp:linux_ui',
        '../../../chrome_resources.gyp:chrome_extra_resources',
        '../../../chrome_resources.gyp:chrome_resources',
        '../../../chrome_resources.gyp:chrome_strings',
        '../../../chrome_resources.gyp:theme_resources',
      ],
      'defines': [
        'LIBGTK2UI_IMPLEMENTATION',
      ],
      # Several of our source files are named _gtk2.cc. This isn't to
      # differentiate them from their source files (ninja and make are sane
      # build systems, unlike MSVS). It is instead to get around the rest of
      # the normal, global gtk exclusion rules, as we are otherwise using gtk
      # in a non-gtk build.
      'sources': [
        'chrome_gtk_frame.cc',
        'chrome_gtk_frame.h',
        'gtk2_ui.cc',
        'gtk2_ui.h',
        'gtk2_util.cc',
        'gtk2_util.h',
        'libgtk2ui_export.h',
        'owned_widget_gtk2.cc',
        'owned_widget_gtk2.h',
        'select_file_dialog_impl.cc',
        'select_file_dialog_impl.h',
        'select_file_dialog_impl_gtk2.cc',
        'select_file_dialog_impl_kde.cc',
        'skia_utils_gtk2.cc',
        'skia_utils_gtk2.h',
      ],
    },
  ],
}
