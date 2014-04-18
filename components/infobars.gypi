# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'infobars_core',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'infobars/core/infobar.cc',
        'infobars/core/infobar.h',
        'infobars/core/infobar_container.cc',
        'infobars/core/infobar_container.h',
        'infobars/core/infobar_delegate.cc',
        'infobars/core/infobar_delegate.h',
        'infobars/core/infobar_manager.cc',
        'infobars/core/infobar_manager.h',
        'infobars/core/infobars_switches.cc',
        'infobars/core/infobars_switches.h',
      ],
    },
  ],
}
