# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'aura',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../skia/skia.gyp:skia',
        '../ui/gfx/compositor/compositor.gyp:compositor',
        '../ui/ui.gyp:gfx_resources',
        '../ui/ui.gyp:ui',
        '../ui/ui.gyp:ui_resources',
      ],
      'sources': [
        'desktop_host.h',
        'desktop_host_linux.cc',
        'desktop_host_win.cc',
        'desktop_host_win.h',
        'desktop.cc',
        'desktop.h',
        'event.cc',
        'event.h',
        'event_win.cc',
        'root_window.cc',
        'root_window.h',
        'window.cc',
        'window.h',
        'window_delegate.h',
      ],
    },
    {
      'target_name': 'aura_demo',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/gfx/compositor/compositor.gyp:compositor',
        '../ui/ui.gyp:gfx_resources',
        '../ui/ui.gyp:ui',
        '../ui/ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'demo/demo_main.cc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
    },
    {
      'target_name': 'aura_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/ui.gyp:gfx_resources',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'window_unittest.cc',
        'run_all_unittests.cc',
        'test_suite.cc',
        'test_suite.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
    },
  ],
}
