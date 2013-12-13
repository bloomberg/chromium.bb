# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'apps',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      # Since browser and browser_extensions actually depend on each other,
      # we must omit the dependency from browser_extensions to browser.
      # However, this means browser_extensions and browser should more or less
      # have the same dependencies. Once browser_extensions is untangled from
      # browser, then we can clean up these dependencies.
      'dependencies': [
        'browser_extensions',
        'common/extensions/api/api.gyp:api',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(grit_out_dir)',
      ],
      'sources': [
        'app_keep_alive_service.cc',
        'app_keep_alive_service.h',
        'app_keep_alive_service_factory.cc',
        'app_keep_alive_service_factory.h',
        'app_lifetime_monitor.cc',
        'app_lifetime_monitor.h',
        'app_lifetime_monitor_factory.cc',
        'app_lifetime_monitor_factory.h',
        'app_load_service.cc',
        'app_load_service.h',
        'app_load_service_factory.cc',
        'app_load_service_factory.h',
        'app_restore_service.cc',
        'app_restore_service.h',
        'app_restore_service_factory.cc',
        'app_restore_service_factory.h',
        'app_window_contents.cc',
        'app_window_contents.h',
        'apps_client.cc',
        'apps_client.h',
        'launcher.cc',
        'launcher.h',
        'metrics_names.h',
        'pref_names.cc',
        'pref_names.h',
        'prefs.cc',
        'prefs.h',
        'saved_files_service.cc',
        'saved_files_service.h',
        'saved_files_service_factory.cc',
        'saved_files_service_factory.h',
        'shell_window.cc',
        'shell_window.h',
        'shell_window_geometry_cache.cc',
        'shell_window_geometry_cache.h',
        'shell_window_registry.cc',
        'shell_window_registry.h',
        'switches.cc',
        'switches.h',
        'ui/native_app_window.h',
        'ui/views/shell_window_frame_view.cc',
        'ui/views/shell_window_frame_view.h',
      ],
      'conditions': [
        ['chromeos==1',
          {
            'dependencies': [
              'browser_chromeos',
            ]
          }
        ],
        ['enable_extensions==0',
          {
            'sources/': [
              ['exclude', '^apps/'],
            ],
          }
        ],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/base/strings/ui_strings.gyp:ui_strings',
            '../ui/views/views.gyp:views',
          ],
        }, {  # toolkit_views==0
          'sources/': [
            ['exclude', 'ui/views/'],
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],  # targets
  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'app_shell',
          'type': 'executable',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'variables': {
            'chromium_code': 1,
          },
          'dependencies': [
            'apps',
            'chrome_resources.gyp:packed_resources',
            'test_support_common',
            '../base/base.gyp:base',
            '../base/base.gyp:base_prefs_test_support',
            '../content/content.gyp:content',
            '../content/content_shell_and_tests.gyp:content_shell_lib',
            '../skia/skia.gyp:skia',
            '../ui/views/views.gyp:views',
            '../ui/wm/wm.gyp:wm_test_support',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/shell_app_sorting.cc',
            'shell/shell_app_sorting.h',
            'shell/shell_browser_context.cc',
            'shell/shell_browser_context.h',
            'shell/shell_browser_main_parts.cc',
            'shell/shell_browser_main_parts.h',
            'shell/shell_content_browser_client.cc',
            'shell/shell_content_browser_client.h',
            'shell/shell_content_client.cc',
            'shell/shell_content_client.h',
            'shell/shell_extensions_browser_client.cc',
            'shell/shell_extensions_browser_client.h',
            'shell/shell_extensions_client.cc',
            'shell/shell_extensions_client.h',
            'shell/shell_main_delegate.cc',
            'shell/shell_main_delegate.h',
            'shell/shell_main.cc',
            'shell/web_view_window.cc',
            'shell/web_view_window.cc',
          ],
        },
      ],  # targets
    }],  # chromeos==1
  ],  # conditions
}
