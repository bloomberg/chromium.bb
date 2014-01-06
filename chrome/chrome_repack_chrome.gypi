# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  # This file is an unused, a mostly empty placeholder which should go away
  # eventually -- http://crbug.com/52609
  'action_name': 'repack_chrome',
  'variables': {
    'pak_inputs': [
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
