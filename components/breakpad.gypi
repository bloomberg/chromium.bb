# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'breakpad_common',
      # TODO(jochen): should be static_library, once there are actual .cc files.
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'breakpad/common/breakpad_paths.h',
      ],
    },
  ],
}
