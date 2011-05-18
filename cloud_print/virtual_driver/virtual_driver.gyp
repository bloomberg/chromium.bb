# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'virtual_driver',
      'type': 'none',
      'dependencies': [
        'win/install/virtual_driver_install.gyp:*',
        'win/port_monitor/virtual_driver_port_monitor.gyp:*',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
