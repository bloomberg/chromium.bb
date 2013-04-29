# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'encryptor',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        'webdata/encryptor/encryptor.h',
        'webdata/encryptor/encryptor_mac.mm',
        'webdata/encryptor/encryptor_password_mac.h',
        'webdata/encryptor/encryptor_password_mac.mm',
        'webdata/encryptor/encryptor_posix.cc',
        'webdata/encryptor/encryptor_win.cc',
        'webdata/encryptor/ie7_password.cc',
        'webdata/encryptor/ie7_password.h',
      ],
      'conditions': [
        ['OS!="win"', {
          'sources!': [
            'webdata/encryptor/ie7_password.cc'
          ],
        }],
        ['OS=="mac"', {
          'sources!': [
            'webdata/encryptor/encryptor_posix.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'webdata_common',
      'type': '<(component)',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../sql/sql.gyp:sql',
      ],
      'defines': [
        'WEBDATA_IMPLEMENTATION',
      ],
      'sources': [
        'webdata/common/web_database.cc',
        'webdata/common/web_database.h',
        'webdata/common/web_database_observer.h',
        'webdata/common/web_database_service.cc',
        'webdata/common/web_database_service.h',
        'webdata/common/web_database_table.cc',
        'webdata/common/web_database_table.h',
        'webdata/common/web_data_request_manager.cc',
        'webdata/common/web_data_request_manager.h',
        'webdata/common/web_data_results.h',
        'webdata/common/web_data_service_backend.cc',
        'webdata/common/web_data_service_backend.h',
        'webdata/common/web_data_service_base.cc',
        'webdata/common/web_data_service_base.h',
        'webdata/common/web_data_service_consumer.h',
        'webdata/common/webdata_constants.cc',
        'webdata/common/webdata_constants.h',
        'webdata/common/webdata_export.h'
      ],
    },
  ],
}
