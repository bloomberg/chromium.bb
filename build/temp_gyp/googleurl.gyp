# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(tfarina): Remove this file when all the references are updated.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'googleurl',
      'type': 'none',
      'dependencies': [
        '../../googleurl/googleurl.gyp:googleurl',
      ],
      'export_dependent_settings': [
        '../../googleurl/googleurl.gyp:googleurl',
      ]
    },
  ],
}
