# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/browser_sync_browser
      'target_name': 'browser_sync_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sync/sync.gyp:sync',
        '../ui/base/ui_base.gyp:ui_base',
        'autofill_core_common',
        'browser_sync_common',
        'components_strings.gyp:components_strings',
        'history_core_browser',
        'invalidation_impl',
        'invalidation_public',
        'keyed_service_core',
        'pref_registry',
        'signin_core_browser',
        'syncable_prefs',
        'sync_driver',
        'sync_sessions',
        'version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/browser/profile_sync_service.cc',
        'browser_sync/browser/profile_sync_service.h',
        'browser_sync/browser/signin_confirmation_helper.cc',
        'browser_sync/browser/signin_confirmation_helper.h',
      ],
    },
    {
      # GN version: //components/browser_sync_common
      'target_name': 'browser_sync_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'browser_sync/common/browser_sync_switches.cc',
        'browser_sync/common/browser_sync_switches.h',
      ],
    },
  ],
}
