# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ui/zoom
      'target_name': 'ui_zoom',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'ui/zoom/page_zoom.cc',
        'ui/zoom/page_zoom.h',
        'ui/zoom/page_zoom_constants.cc',
        'ui/zoom/page_zoom_constants.h',
        'ui/zoom/zoom_controller.cc',
        'ui/zoom/zoom_controller.h',
        'ui/zoom/zoom_event_manager.cc',
        'ui/zoom/zoom_event_manager.h',
        'ui/zoom/zoom_event_manager_observer.h',
        'ui/zoom/zoom_observer.h'
      ],
    },
    {
      'target_name': 'ui_zoom_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../components/components.gyp:ui_zoom',
      ],
      'sources': [
        'ui/zoom/test/zoom_test_utils.cc',
        'ui/zoom/test/zoom_test_utils.h'
      ],
    }
  ],
}
