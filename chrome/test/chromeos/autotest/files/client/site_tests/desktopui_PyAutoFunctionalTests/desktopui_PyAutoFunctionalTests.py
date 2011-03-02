# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import constants, chrome_test, cros_ui


class desktopui_PyAutoFunctionalTests(chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based functional tests.

    Performs all setup and fires off the FULL suite.
    """
    version = 1

    def initialize(self):
        chrome_test.ChromeTestBase.initialize(self)
        assert os.geteuid() == 0, 'Need superuser privileges'

        deps_dir = os.path.join(self.autodir, 'deps')
        subprocess.check_call(['chown', '-R', 'chronos', self.cr_source_dir])

        # Setup /tmp/disable_chrome_restart
        if not os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE):
          open(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE, 'w').close()

        # Setup suid python binary which can enable chrome testing interface
        suid_python = os.path.join(self.test_binary_dir, 'python')
        py_path = subprocess.Popen(['which', 'python'],
                                   stdout=subprocess.PIPE).communicate()[0]
        py_path = py_path.strip()
        assert os.path.exists(py_path), 'Could not find python'
        if os.path.islink(py_path):
          linkto = os.readlink(py_path)
          py_path = os.path.join(os.path.dirname(py_path), linkto)
        shutil.copy(py_path, suid_python)
        os.chown(suid_python, 0, 0)
        os.chmod(suid_python, 04755)

    def run_once(self):
        # Enable chrome testing interface and Login
        deps_dir = os.path.join(self.autodir, 'deps')
        pyautolib_dir = os.path.join(self.cr_source_dir,
                                     'chrome', 'test', 'pyautolib')
        login_cmd = cros_ui.xcommand_as(
            'python %s chromeos_utils.ChromeosUtils.LoginToDefaultAccount '
            '-v --no-http-server' %
                os.path.join(pyautolib_dir, 'chromeos', 'chromeos_utils.py'))
        utils.system(login_cmd)

        # Run pyauto tests in the "FULL" suite
        functional_cmd = cros_ui.xcommand_as(
            '%s/chrome_test/test_src/chrome/test/functional/' \
            'pyauto_functional.py --suite=FULL ' \
            '-v --no-http-server' % deps_dir)
        utils.system(functional_cmd)
