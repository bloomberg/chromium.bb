# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
    '../..'
    ],
  },
  'targets' : [
    {
      'target_name': 'GCP-driver',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'msvs_guid': '8D06D53B-289E-4f99-99FC-77C77DB478A8',
      'sources' : [
        'virtual_driver_posix.cc',
        'printer_driver_util_linux.cc',
        '../virtual_driver_switches.cc',
      ]
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
