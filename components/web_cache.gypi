# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'web_cache_common',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/ipc/ipc.gyp:ipc',
      ],
      'sources': [
        'web_cache/common/web_cache_message_generator.cc',
        'web_cache/common/web_cache_message_generator.h',
        'web_cache/common/web_cache_messages.h',
      ],
    },
    {
      'target_name': 'web_cache_browser',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        'web_cache_common',
      ],
      'sources': [
        'web_cache/browser/web_cache_manager.cc',
        'web_cache/browser/web_cache_manager.h',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'web_cache_renderer',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        'web_cache_common',
      ],
      'sources': [
        'web_cache/renderer/web_cache_render_process_observer.cc',
        'web_cache/renderer/web_cache_render_process_observer.h',
      ],
    },
  ],
}
