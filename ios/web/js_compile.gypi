# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # TODO(ios): Enable javascript compilation. See http://crbug.com/429756
  'rules': [
    {
      'rule_name': 'jscompilation',
      'extension': 'js',
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_NAME)',
      ],
      'action': [
        'cp',
        '<(RULE_INPUT_PATH)',
        '<@(_outputs)',
      ],
    },
  ],  # rule_name: jscompilation
}
