# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from autotest_lib.client.cros import chrome_test

class desktopui_PyAutoFunctionalTests(chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based functional tests."""
    version = 1

    def run_once(self):
        deps_dir = os.path.join(self.autodir, 'deps')
        self.test_binary_dir = ''
        pyauto_script = '%s/chrome_test/test_src/chrome/test/functional/' \
                        'pyauto_functional.py' % deps_dir
        self.run_chrome_test(pyauto_script)
