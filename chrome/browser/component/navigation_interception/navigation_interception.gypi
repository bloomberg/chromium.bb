# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(jknotten): Remove this file once external dependencies have changed to
# use the new location of this component at
# content/components/navigation_interception.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'navigation_interception_java',
          'type': 'none',
          'dependencies': [
            '../../../content/content.gyp:navigation_interception_java',
          ],
        },
      ],
    }],
  ],
}
