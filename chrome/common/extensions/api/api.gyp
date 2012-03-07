# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'api',
      'type': 'static_library',
      'sources': [
        '<@(json_schema_files)',
        '<@(idl_schema_files)',
      ],
      'includes': ['../../../../build/json_schema_compile.gypi'],
      'variables': {
        'chromium_code': 1,
        'json_schema_files': [
          'experimental.declarative.json',
          'permissions.json',
          'tabs.json',
          'windows.json',
        ],
        'idl_schema_files': [
          'experimental.dns.idl',
        ],
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',
      },
    },
  ],
}
