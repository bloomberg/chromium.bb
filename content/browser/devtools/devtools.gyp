# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_protocol_handler',
      'type': 'none',
      'actions': [
        {
          'action_name': 'devtools_protocol_handler',
          'variables': {
            'blink_protocol': '../../../third_party/WebKit/Source/devtools/protocol.json',
            'generator': 'protocol/devtools_protocol_handler_generator.py',
            'output_cc': '<(SHARED_INTERMEDIATE_DIR)/content/browser/devtools/protocol/devtools_protocol_handler_impl.cc',
            'output_h': '<(SHARED_INTERMEDIATE_DIR)/content/browser/devtools/protocol/devtools_protocol_handler_impl.h',
          },
          'inputs': [
            '<(blink_protocol)',
            '<(generator)',
          ],
          'outputs': [
            '<(output_cc)',
            '<(output_h)',
          ],
          'action':[
            'python',
            '<(generator)',
            '<(blink_protocol)',
            '<(output_cc)',
            '<(output_h)',
          ],
          'message': 'Generating DevTools protocol browser-side handlers from <(blink_protocol)'
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ]
      },
    },
  ],
}
