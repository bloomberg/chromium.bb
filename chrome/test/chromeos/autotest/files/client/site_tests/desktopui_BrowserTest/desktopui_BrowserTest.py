# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros import chrome_test, cros_ui, cros_ui_test
from blacklists import blacklist, blacklist_vm

SKIP_DEPS_ARG = 'skip_deps'
GDBSERVER_ARG = 'gdbserver'
GTEST_FILTER_ARG = 'gtest_filter='

GDBSERVER_SETUP_CMD = '/sbin/iptables -A INPUT  -p tcp --dport 1234 -j ACCEPT'
GDBSERVER_STRING = 'gdbserver PORT:1234'

def get_binary_prefix(arguments=[]):
    if GDBSERVER_ARG in arguments:
        cros_ui.xsystem(GDBSERVER_SETUP_CMD, ignore_status=True)
        return GDBSERVER_STRING + ' '
    return ''


class desktopui_BrowserTest(chrome_test.ChromeTestBase, cros_ui_test.UITest):
    version = 1
    binary_to_run='browser_tests'

    def initialize(self, creds='$default', arguments=[]):
        if SKIP_DEPS_ARG in arguments:
            skip_deps = True
        else:
            skip_deps = False
        chrome_test.ChromeTestBase.initialize(self, False, skip_deps=skip_deps)
        cros_ui_test.UITest.initialize(self, creds)

    def cleanup(self):
        cros_ui_test.UITest.cleanup(self)
        chrome_test.ChromeTestBase.cleanup(self)

    def get_tests_to_run(self, group=0, total_groups=1, arguments=[]):
        # Tests specified in arguments override default test behavior
        tests_to_run = [x[len(GTEST_FILTER_ARG):]
                        for x in arguments
                        if GTEST_FILTER_ARG in x]
        if not tests_to_run:
            tests_to_run = self.filter_bad_tests(
                self.generate_test_list(self.binary_to_run, group,
                                        total_groups),
                blacklist + blacklist_vm)
        return tests_to_run

    def run_once(self, group=0, total_groups=1, arguments=[]):
        tests_to_run = self.get_tests_to_run(group, total_groups, arguments)
        self.run_chrome_test(self.binary_to_run,
                             '--gtest_filter=%s' % ':'.join(tests_to_run),
                             prefix=get_binary_prefix(arguments))
