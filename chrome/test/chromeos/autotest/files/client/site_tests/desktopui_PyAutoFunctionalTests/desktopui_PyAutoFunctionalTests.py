# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import site_chrome_test

class desktopui_PyAutoFunctionalTests(site_chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based functional tests."""
    version = 1

    def run_once(self):
        pyauto_script = '%s/test_src/chrome/test/functional/' \
                        'pyauto_functional.py' % self.dep_dir
        self.run_chrome_test(pyauto_script)
