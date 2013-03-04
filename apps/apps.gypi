# Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
      ],
      'sources': [
        'app_launcher.cc',
        'app_launcher.h',
        'app_restore_service.cc',
        'app_restore_service.h',
        'app_restore_service_factory.cc',
        'app_restore_service_factory.h',
        'pref_names.cc',
        'pref_names.h',
        'prefs.cc',
        'prefs.h',
      ],
      'conditions': [
        ['enable_extensions==0', {
          'sources/': [
            ['exclude', '^apps/'],
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
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
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/chrome/chrome.gyp:chrome_version_resources',
            '<(DEPTH)/chrome/chrome.gyp:launcher_support',
            '<(DEPTH)/google_update/google_update.gyp:google_update',
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
    },],  # 'OS=="win"'
  ],  # 'conditions'
}
