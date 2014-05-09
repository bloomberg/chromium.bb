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
        'invalidation/ticl_settings_provider.cc',
        'invalidation/ticl_settings_provider.h',
      ],
    },
  ],
}
