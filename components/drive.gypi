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
        'drive_proto',
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
        'drive/drive_pref_names.cc',
        'drive/drive_pref_names.h',
        'drive/drive_uploader.cc',
        'drive/drive_uploader.h',
        'drive/event_logger.cc',
        'drive/event_logger.h',
        'drive/file_change.cc',
        'drive/file_change.h',
        'drive/file_errors.cc',
        'drive/file_errors.h',
        'drive/local_file_reader.cc',
        'drive/local_file_reader.h',
        'drive/service/drive_api_service.cc',
        'drive/service/drive_api_service.h',
        'drive/service/drive_service_interface.cc',
        'drive/service/drive_service_interface.h',
      ],
    },

    {
      # GN version: //components/drive:proto
      # Protobuf compiler / generator for the Drive protocol buffer.
      'target_name': 'drive_proto',
      'type': 'static_library',
      'sources': [ 'drive/drive.proto' ],
      'variables': {
        'proto_in_dir': 'drive',
        'proto_out_dir': 'components/drive',
      },
      'includes': [ '../build/protoc.gypi' ]
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
        'drive_proto',
        '../base/base.gyp:base',
        '../content/content_shell_and_tests.gyp:test_support_content',
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
        "drive/drive_test_util.cc",
        "drive/drive_test_util.h",
      ],
    },

    # TODO(lukasza): drive_unittests target.
    # Currently tests are built as part of chrome/chrome_tests_unit.gypi.
  ],
}
