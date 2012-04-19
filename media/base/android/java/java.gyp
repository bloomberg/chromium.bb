# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'media_java',
      'message': 'building media java sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'media_java',
          'inputs': [
            'media.xml',
            'org/chromium/media/MediaPlayerListener.java',
          ],
          'dependencies': [
            '<(DEPTH)/base/android/java/java.gyp:base_java',
          ],
          'outputs': [
            '$(PRODUCT_DIR)/chromium_media.jar',
          ],
          'action': [
            'ant',
            '-DPRODUCT_DIR=<(PRODUCT_DIR)',
            '-buildfile',
            'media.xml',
          ]
        },
      ],
    },
  ],
}
