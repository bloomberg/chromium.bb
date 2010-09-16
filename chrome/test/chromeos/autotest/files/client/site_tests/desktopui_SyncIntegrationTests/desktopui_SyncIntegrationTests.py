# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from autotest_lib.client.bin import site_chrome_test

class desktopui_SyncIntegrationTests(site_chrome_test.ChromeTestBase):
    version = 1

    def run_once(self):
        dep = 'chrome_test'
        dep_dir = os.path.join(self.autodir, 'deps', dep)
        password_file = '%s/sync_password.txt' % dep_dir

        self.run_chrome_test('sync_integration_tests',
            ('--password-file-for-test=%s ' +
             '--test-terminate-timeout=300000') % password_file)

