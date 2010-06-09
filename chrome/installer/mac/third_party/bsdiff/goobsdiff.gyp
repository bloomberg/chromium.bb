# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'type': 'executable',
    'link_settings': {
      'libraries': [
        '$(SDKROOT)/usr/lib/libbz2.dylib',
        '$(SDKROOT)/usr/lib/libz.dylib',
        '$(SDKROOT)/usr/lib/libcrypto.dylib',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'goobsdiff',
      'sources': [
        'goobsdiff.c',
      ],
    },
    {
      'target_name': 'goobspatch',
      'sources': [
        'goobspatch.c',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
