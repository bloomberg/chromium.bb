# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'invalidation',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../jingle/jingle.gyp:notifier',
        'keyed_service_core',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'invalidation/invalidation_logger.cc',
        'invalidation/invalidation_logger.h',
        'invalidation/invalidation_logger_observer.h',
        'invalidation/invalidation_service.h',
        'invalidation/invalidation_service_util.cc',
        'invalidation/invalidation_service_util.h',
        'invalidation/invalidation_switches.cc',
        'invalidation/invalidation_switches.h',
        'invalidation/ticl_settings_provider.cc',
        'invalidation/ticl_settings_provider.h',
      ],
    },

    {
      'target_name': 'invalidation_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../net/net.gyp:net',
        '../sync/sync.gyp:sync',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        'keyed_service_core',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'invalidation/p2p_invalidation_service.cc',
        'invalidation/p2p_invalidation_service.h',
      ],
    },
  ],
}
