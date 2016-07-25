# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/spellcheck/common
      'target_name': 'spellcheck_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'spellcheck/common/spellcheck_switches.cc',
        'spellcheck/common/spellcheck_switches.h',
      ],
    },
  ],
}
