# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_browsing_db
      'target_name': 'safe_browsing_db',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'safe_browsing_db/prefix_set.h',
        'safe_browsing_db/prefix_set.cc',
        'safe_browsing_db/safe_browsing_db_util.h',
        'safe_browsing_db/safe_browsing_db_util.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
