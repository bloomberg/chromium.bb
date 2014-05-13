# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gcm_driver',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/gcm/gcm.gyp:gcm',
        'os_crypt',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'gcm_driver/default_gcm_app_handler.cc',
        'gcm_driver/default_gcm_app_handler.h',
        'gcm_driver/gcm_app_handler.h',
        'gcm_driver/gcm_client_factory.cc',
        'gcm_driver/gcm_client_factory.h',
        'gcm_driver/system_encryptor.cc',
        'gcm_driver/system_encryptor.h',
      ],
    },
  ],
}
