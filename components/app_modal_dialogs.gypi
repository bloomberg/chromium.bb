# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'app_modal_dialogs',
      'type': 'static_library',
      'dependencies': [
	'../skia/skia.gyp:skia',
	'../content/content.gyp:content_browser',
        'components_strings.gyp:components_strings',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app_modal_dialogs/app_modal_dialog.cc',
        'app_modal_dialogs/app_modal_dialog.h',
        'app_modal_dialogs/app_modal_dialog_test_util.h',
        'app_modal_dialogs/app_modal_dialog_queue.cc',
        'app_modal_dialogs/app_modal_dialog_queue.h',
        'app_modal_dialogs/javascript_app_modal_dialog.cc',
        'app_modal_dialogs/javascript_app_modal_dialog.h',
        'app_modal_dialogs/native_app_modal_dialog.h'
      ],
      'conditions': [
        ['use_aura==1',{
          'dependencies': [
            '../ui/aura/aura.gyp:aura',
          ],
        }]
      ],
    },
  ],
}
