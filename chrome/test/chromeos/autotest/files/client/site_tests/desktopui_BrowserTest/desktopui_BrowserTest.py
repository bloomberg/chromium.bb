# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import site_chrome_test

class desktopui_BrowserTest(site_chrome_test.ChromeTestBase):
    version = 1

    binary_to_run='browser_tests'
    blacklist = []

    def run_once(self, group=0, total_groups=4):
        tests_to_run = self.filter_bad_tests(
            self.generate_test_list(self.binary_to_run, group, total_groups))
        tests_to_run.extend(map(lambda(x): '-'+x, self.blacklist))
        self.run_chrome_test(self.binary_to_run,
                             '--gtest_filter=%s' % ':'.join(tests_to_run))
