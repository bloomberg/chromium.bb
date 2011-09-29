# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'sync_proto',
      'type': 'static_library',
      'sources': [
        'sync.proto',
        'encryption.proto',
        'app_notification_specifics.proto',
        'app_specifics.proto',
        'autofill_specifics.proto',
        'bookmark_specifics.proto',
        'extension_setting_specifics.proto',
        'extension_specifics.proto',
        'nigori_specifics.proto',
        'password_specifics.proto',
        'preference_specifics.proto',
        'search_engine_specifics.proto',
        'session_specifics.proto',
        'test.proto',
        'theme_specifics.proto',
        'typed_url_specifics.proto',
      ],
      'variables': {
        'proto_out_dir': 'chrome/browser/sync/protocol',
      },
      'includes': ['../../../../build/protoc.gypi'],
    },
  ],
}
