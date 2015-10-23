# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'arch_suffix': '<(arch_suffix)',
    'variables': {
      'conditions': [
        ['target_arch=="arm" or target_arch=="ia32" or target_arch=="mipsel"', {
          'arch_suffix': '32',
        }, {
          'arch_suffix': '64',
        }],
      ],
    }
  }
}
