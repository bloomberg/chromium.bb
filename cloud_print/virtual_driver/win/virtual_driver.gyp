# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'virtual_driver_suffix': '',
  },
  'target_defaults': {
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
    ],

  },
  'includes': [
    'virtual_driver.gypi',
  ],
}
