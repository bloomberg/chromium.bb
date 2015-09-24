# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/flags_ui
      'target_name': 'flags_ui',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'flags_ui/flags_ui_constants.cc',
        'flags_ui/flags_ui_constants.h',
      ],
    },
  ],
}
