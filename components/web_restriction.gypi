# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/web_restriction:web_restriction_java
          'target_name': 'web_restriction_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'web_restriction/java',
          },
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'includes': [ '../build/java.gypi' ],
        },
      ],
  }]]
}
