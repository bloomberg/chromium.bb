# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1 and disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'chromevox2_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/cvox2/background',
              'files': [
                'cvox2/background/background.html',
                'cvox2/background/background.js',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/cvox2/injected',
              'files': [
                'cvox2/injected/injected.js',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
