# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'autofill_regexes',
      'type': 'none',
      'actions': [{
        'action_name': 'autofill_regexes',
        'inputs': [
          '<(DEPTH)/build/escape_unicode.py',
          'autofill/browser/autofill_regex_constants.cc.utf8',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/autofill_regex_constants.cc',
        ],
        'action': ['python', '<(DEPTH)/build/escape_unicode.py',
                   '-o', '<(SHARED_INTERMEDIATE_DIR)',
                   'autofill/browser/autofill_regex_constants.cc.utf8'],
      }],
    },
    {
      # Protobuf compiler / generate rule for Autofill's risk integration.
      'target_name': 'autofill_risk_proto',
      'type': 'static_library',
      'sources': [
        'autofill/browser/risk/proto/fingerprint.proto',
      ],
      'variables': {
        'proto_in_dir': 'autofill/browser/risk/proto',
        'proto_out_dir': 'components/autofill/browser/risk/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'autofill_common',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../build/temp_gyp/googleurl.gyp:googleurl',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
            '../ui/ui.gyp:ui',
          ],
          'sources': [
            'autofill/common/autocheckout_status.h',
            'autofill/common/autofill_constants.cc',
            'autofill/common/autofill_constants.h',
            'autofill/common/autofill_messages.h',
            'autofill/common/autofill_pref_names.cc',
            'autofill/common/autofill_pref_names.h',
            'autofill/common/autofill_switches.cc',
            'autofill/common/autofill_switches.h',
            'autofill/common/form_data.cc',
            'autofill/common/form_data.h',
            'autofill/common/form_data_predictions.cc',
            'autofill/common/form_data_predictions.h',
            'autofill/common/form_field_data.cc',
            'autofill/common/form_field_data.h',
            'autofill/common/form_field_data_predictions.cc',
            'autofill/common/form_field_data_predictions.h',
            'autofill/common/password_form_fill_data.cc',
            'autofill/common/password_form_fill_data.h',
            'autofill/common/password_generation_util.cc',
            'autofill/common/password_generation_util.h',
            'autofill/common/web_element_descriptor.cc',
            'autofill/common/web_element_descriptor.h',
          ],
        },
      ],
    }],
  ],
}
