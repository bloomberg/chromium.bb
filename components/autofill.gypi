# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
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
            'autofill/common/web_element_descriptor.cc',
            'autofill/common/web_element_descriptor.h',
          ],
        },
      ],
    }],
  ],
}
