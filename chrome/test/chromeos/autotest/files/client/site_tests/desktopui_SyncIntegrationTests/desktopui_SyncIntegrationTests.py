# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros import chrome_test

class desktopui_SyncIntegrationTests(chrome_test.ChromeTestBase):
    version = 1

    binary_to_run = 'sync_integration_tests'
    cmd_line_params = '--test-terminate-timeout=120000'

    def run_once(self):
        self.run_chrome_test(self.binary_to_run, self.cmd_line_params)
