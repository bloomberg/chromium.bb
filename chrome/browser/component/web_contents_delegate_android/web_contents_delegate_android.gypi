# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(jknotten): Remove this file once external dependencies have changed to
# use the new location of this component at
# content/components/web_contents_delegate_android_java

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'web_contents_delegate_android_java',
          'type': 'none',
          'dependencies': [
            '../../../content/content.gyp:web_contents_delegate_android_java',
          ],
        },
      ],
    }],
  ],
}
