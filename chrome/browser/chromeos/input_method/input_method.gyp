# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'input_method_out_dir':
      '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/chromeos/input_method',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
  },
  'targets': [
    {
      'target_name': 'gencode',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'ibus_input_methods.txt',
            'gen_ibus_input_methods.py',
          ],
          'outputs': [
            '<(input_method_out_dir)/ibus_input_methods.h',
          ],
          'action_name': 'gen_ibus_input_methods',
          'action': [
            'python',
            'gen_ibus_input_methods.py',
            'ibus_input_methods.txt',
            '<(input_method_out_dir)/ibus_input_methods.h',
          ],
          'message': 'Generating ibus_input_methods.h',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ]
}
