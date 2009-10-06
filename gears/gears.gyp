# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    [ 'OS == "win"', {
      'targets': [
        {
          'target_name': 'gears',
          'type': 'none',
          'msvs_guid': 'D703D7A0-EDC1-4FE6-9E22-56154155B24E',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'binaries/gears.dll',
                'binaries/gears.pdb',
              ],
            },
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
