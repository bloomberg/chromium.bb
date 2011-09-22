# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros import chrome_test
from autotest_lib.client.cros import cros_ui_test
from blacklists import blacklist, blacklist_vm

class desktopui_UITest(chrome_test.ChromeTestBase, cros_ui_test.UITest):
    version = 1
    binary_to_run='ui_tests'

    def initialize(self, creds='$default'):
        chrome_test.ChromeTestBase.initialize(self, False)
        cros_ui_test.UITest.initialize(self, creds)

    def cleanup(self):
        cros_ui_test.UITest.cleanup(self)
        chrome_test.ChromeTestBase.cleanup(self)

    def run_once(self, group=0, total_groups=4):
        tests_to_run = self.filter_bad_tests(
            self.generate_test_list(self.binary_to_run, group, total_groups),
            blacklist + blacklist_vm)
        self.run_chrome_test(self.binary_to_run,
                             '--gtest_filter=%s' % ':'.join(tests_to_run))
