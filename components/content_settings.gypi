# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/content_settings/core/common
      'target_name': 'content_settings_core_common',
      'type': 'none',
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'content_settings/core/common/content_settings_types.h',
      ],
    },
  ],
}
