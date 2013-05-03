# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'chromeos_memory',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'defines': [
        'CHROMEOS_MEMORY_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'memory/low_memory_listener.cc',
        'memory/low_memory_listener.h',
        'memory/low_memory_listener_delegate.h',
      ],
    },
  ],
}
