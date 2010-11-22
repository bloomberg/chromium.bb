# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="win"', {
        'python': [
          '<(DEPTH)\\third_party\\python_24\\setup_env.bat && python'
        ],
      }, { # OS != win
        'python': [
          'python'
        ],
      }],
    ],
  },
}
