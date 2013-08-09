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
        'app_launch_for_metro_restart_win.cc',
        'app_launch_for_metro_restart_win.h',
        'app_launcher.cc',
        'app_launcher.h',
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
        'app_shim/app_shim_handler_mac.cc',
        'app_shim/app_shim_handler_mac.h',
        'app_shim/app_shim_host_mac.cc',
        'app_shim/app_shim_host_mac.h',
        'app_shim/app_shim_host_manager_mac.h',
        'app_shim/app_shim_host_manager_mac.mm',
        'app_shim/app_shim_mac.cc',
        'app_shim/app_shim_mac.h',
        'app_shim/chrome_main_app_mode_mac.mm',
        'app_shim/extension_app_shim_handler_mac.cc',
        'app_shim/extension_app_shim_handler_mac.h',
        'app_window_contents.cc',
        'app_window_contents.h',
        'field_trial_names.cc',
        'field_trial_names.h',
        'launcher.cc',
        'launcher.h',
        'metrics_names.h',
        'native_app_window.h',
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
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'app_host',
            'type': 'executable',
            'include_dirs': [
              '..',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '..',
              ],
            },
            'dependencies': [
              '../base/base.gyp:base',
              '../chrome/chrome.gyp:chrome_version_resources',
              '../chrome/chrome.gyp:launcher_support',
              '../google_update/google_update.gyp:google_update',
            ],
            'sources': [
              'app_host/app_host.rc',
              'app_host/app_host_main.cc',
              'app_host/app_host_resource.h',
              'app_host/binaries_installer.cc',
              'app_host/binaries_installer.h',
              'app_host/update.cc',
              'app_host/update.h',
              '<(SHARED_INTERMEDIATE_DIR)/chrome_version/app_host_exe_version.rc',
            ],
            'msvs_settings': {
              'VCLinkerTool': {
                'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
              },
            },
          },
        ],
      },
    ],  # 'OS=="win"'
  ],  # 'conditions'
}
