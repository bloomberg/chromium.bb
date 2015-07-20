# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/drive:drive
      'target_name': 'drive',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:invalidation_public',

        # TODO(lukasza): Remove this dependency (see DEPS file for more info).
        '../content/content.gyp:content_browser',

        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',

        # TODO(lukasza): Remove this dependency (see DEPS file for more info).
        '../storage/storage_browser.gyp:storage',

        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        '../third_party/re2/re2.gyp:re2',
      ],
      'sources': [
        'drive/drive_api_util.cc',
        'drive/drive_api_util.h',
        'drive/drive_app_registry.cc',
        'drive/drive_app_registry.h',
        'drive/drive_app_registry_observer.h',
        'drive/drive_notification_manager.cc',
        'drive/drive_notification_manager.h',
        'drive/drive_notification_observer.h',
        'drive/drive_uploader.cc',
        'drive/drive_uploader.h',
        'drive/event_logger.cc',
        'drive/event_logger.h',
        'drive/service/drive_api_service.cc',
        'drive/service/drive_api_service.h',
        'drive/service/drive_service_interface.cc',
        'drive/service/drive_service_interface.h',
      ],
    },

    {
      # GN version: //components/drive:test_support
      'target_name': 'drive_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'drive',
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "drive/service/dummy_drive_service.cc",
        "drive/service/dummy_drive_service.h",
        "drive/service/fake_drive_service.cc",
        "drive/service/fake_drive_service.h",
        "drive/service/test_util.cc",
        "drive/service/test_util.h",
      ],
    },

    # TODO(lukasza): drive_unittests target.
    # Currently tests are built as part of chrome/chrome_tests_unit.gypi.
    # Drive files that probably should be moved out of chrome_tests_unit.gypi:
    #   components/drive/service/drive_api_util_unittest.cc
    #   components/drive/service/drive_app_registry_unittest.cc
    #   components/drive/service/drive_uploader_unittest.cc
    #   components/drive/service/event_logger_unittest.cc
    #   components/drive/service/fake_drive_service_unittest.cc
  ],
}
