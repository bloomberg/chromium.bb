# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'googleurl',
      'type': 'none',
      'dependencies': [
        '../../url/url.gyp:url',
      ],
      'export_dependent_settings': [
        '../../url/url.gyp:url',
      ],
    },
  ],
}
