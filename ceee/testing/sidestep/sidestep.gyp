# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '../../..',
    ],
  },
  'targets': [
    {
      'target_name': 'sidestep',
      'type': 'static_library',
      'dependencies': [],
      'msvs_guid': '92E7FB64-89B3-49B0-BEB5-5FBC39813266',
      'sources': [
        'auto_testing_hook.h',
        'documentation.h',
        'ia32_modrm_map.cc',
        'ia32_opcode_map.cc',
        'integration.h',
        'mini_disassembler.cc',
        'mini_disassembler.h',
        'mini_disassembler_types.h',
        'preamble_patcher.cc',
        'preamble_patcher.h',
        'preamble_patcher_test.cc',
        'preamble_patcher_with_stub.cc',
      ],
    },
  ]
}
