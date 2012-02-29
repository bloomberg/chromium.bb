# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'base_java',
      'message': 'building base java sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'base_java',
          'inputs': [
            '<(DEPTH)/base/android/java/base.xml',
            '<!@(find . -name "*.java")'
          ],
          'outputs': [
            '<(DEPTH)/base/android/java/dist/lib/chromium_base.jar',
          ],
          'action': [
            'ant',
            '-buildfile',
            '<(DEPTH)/base/android/java/base.xml',
          ]
        },
      ],
    },
  ],
}
