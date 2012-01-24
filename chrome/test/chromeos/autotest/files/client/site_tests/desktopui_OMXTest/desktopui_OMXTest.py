# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from autotest_lib.client.cros import chrome_test

class desktopui_OMXTest(chrome_test.ChromeTestBase):
  version = 1

  def run_once(self):
    test_video_file = os.path.join(self.cr_source_dir, 'content', 'common',
                                   'gpu', 'testdata', 'test-25fps.h264')
    # The FPS expectations here are lower than observed in most runs to keep
    # the bots green.
    cmd_line_params = ('--test_video_data="%s:320:240:250:258:35:150:1"' %
                       test_video_file)
    self.run_chrome_test('omx_video_decode_accelerator_unittest',
                         cmd_line_params)
