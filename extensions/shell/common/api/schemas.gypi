# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'chromium_code': 1,
    'non_compiled_schema_files': [
    ],
    'schema_files': [
      'shell.idl',
    ],
    'cc_dir': 'extensions/shell/common/api',
    'root_namespace': 'extensions::shell_api::%(namespace)s',
    'impl_dir_': 'extensions/shell/browser/api',
  },
}
