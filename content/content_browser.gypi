# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_browser',
      'type': '<(library)',
      'dependencies': [
        # Don't include now since it's empty and so will cause a linker error.
        #'content_common',
        '../app/app.gyp:app_resources',
        '../skia/skia.gyp:skia',
        '../ui/ui.gyp:ui_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/tab_contents/background_contents.cc',
        'browser/tab_contents/background_contents.h',
        'browser/tab_contents/constrained_window.h',
        'browser/tab_contents/infobar_delegate.cc',
        'browser/tab_contents/infobar_delegate.h',
        'browser/tab_contents/interstitial_page.cc',
        'browser/tab_contents/interstitial_page.h',
        'browser/tab_contents/language_state.cc',
        'browser/tab_contents/language_state.h',
        'browser/tab_contents/navigation_controller.cc',
        'browser/tab_contents/navigation_controller.h',
        'browser/tab_contents/navigation_entry.cc',
        'browser/tab_contents/navigation_entry.h',
        'browser/tab_contents/page_navigator.h',
        'browser/tab_contents/provisional_load_details.cc',
        'browser/tab_contents/provisional_load_details.h',
        'browser/tab_contents/render_view_host_manager.cc',
        'browser/tab_contents/render_view_host_manager.h',
        'browser/tab_contents/tab_contents.cc',
        'browser/tab_contents/tab_contents.h',
        'browser/tab_contents/tab_contents_delegate.cc',
        'browser/tab_contents/tab_contents_delegate.h',
        'browser/tab_contents/tab_contents_observer.cc',
        'browser/tab_contents/tab_contents_observer.h',
        'browser/tab_contents/tab_contents_view.cc',
        'browser/tab_contents/tab_contents_view.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': '639DB58D-32C2-435A-A711-65A12F62E442',
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
}
