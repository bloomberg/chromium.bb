# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sync_driver',
      'type': 'static_library',
      'dependencies': [
        'encryptor',
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'sync_driver/backend_data_type_configurer.cc',
        'sync_driver/backend_data_type_configurer.h',
        'sync_driver/data_type_controller.cc',
        'sync_driver/data_type_controller.h',
        'sync_driver/data_type_encryption_handler.cc',
        'sync_driver/data_type_encryption_handler.h',
        'sync_driver/data_type_error_handler.h',
        'sync_driver/data_type_manager.cc',
        'sync_driver/data_type_manager.h',
        'sync_driver/data_type_manager_observer.h',
        'sync_driver/failed_data_types_handler.cc',
        'sync_driver/failed_data_types_handler.h',
        'sync_driver/model_association_manager.cc',
        'sync_driver/model_association_manager.h',
        'sync_driver/model_associator.h',
        'sync_driver/proxy_data_type_controller.cc',
        'sync_driver/proxy_data_type_controller.h',
        'sync_driver/sync_frontend.cc',
        'sync_driver/sync_frontend.h',
        'sync_driver/system_encryptor.cc',
        'sync_driver/system_encryptor.h',
        'sync_driver/user_selectable_sync_type.h',
      ],
    },
    {
      'target_name': 'sync_driver_test_support',
      'type': 'static_library',
      'dependencies': [
        'sync_driver',
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'sync_driver/data_type_controller_mock.cc',
        'sync_driver/data_type_controller_mock.h',
        'sync_driver/data_type_error_handler_mock.cc',
        'sync_driver/data_type_error_handler_mock.h',
        'sync_driver/data_type_manager_mock.cc',
        'sync_driver/data_type_manager_mock.h',
        'sync_driver/fake_data_type_controller.cc',
        'sync_driver/fake_data_type_controller.h',
        'sync_driver/model_associator_mock.cc',
        'sync_driver/model_associator_mock.h',
      ],
    },
  ],
}
