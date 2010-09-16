# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import site_chrome_test

class desktopui_BrowserTest(site_chrome_test.ChromeTestBase):
    version = 1

    def run_once(self):
        self.run_chrome_test('browser_tests', '')

