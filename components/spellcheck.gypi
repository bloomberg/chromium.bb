# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/spellcheck/common
      'target_name': 'spellcheck_common',
      'type': 'static_library',
      'dependencies': [
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        "spellcheck/common/spellcheck_bdict_language.h",
        "spellcheck/common/spellcheck_common.cc",
        "spellcheck/common/spellcheck_common.h",
        "spellcheck/common/spellcheck_marker.h",
        "spellcheck/common/spellcheck_message_generator.cc",
        "spellcheck/common/spellcheck_message_generator.h",
        "spellcheck/common/spellcheck_messages.h",
        "spellcheck/common/spellcheck_result.h",
        'spellcheck/common/spellcheck_switches.cc',
        'spellcheck/common/spellcheck_switches.h',
      ],
    },
  ],
}
