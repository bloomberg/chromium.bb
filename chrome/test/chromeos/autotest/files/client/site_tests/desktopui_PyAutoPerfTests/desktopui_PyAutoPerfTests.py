# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pwd
import shutil
import subprocess

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import constants, chrome_test, cros_ui, login


class desktopui_PyAutoPerfTests(chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based performance tests.

    Performs all setup and fires off the CHROMEOS_PERF PyAuto suite.
    """
    version = 1

    def initialize(self):
        chrome_test.ChromeTestBase.initialize(self)
        assert os.geteuid() == 0, 'Need superuser privileges'

        deps_dir = os.path.join(self.autodir, 'deps')
        subprocess.check_call(['chown', '-R', 'chronos', self.cr_source_dir])

        # Setup suid python binary which can enable Chrome testing interface.
        suid_python = os.path.join(self.test_binary_dir, 'suid-python')
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

        # User chronos should own the current directory.
        chronos_id = pwd.getpwnam('chronos')
        os.chown(os.getcwd(), chronos_id.pw_uid, chronos_id.pw_gid)

        # Make sure Chrome minidumps are written locally.
        minidumps_file = '/mnt/stateful_partition/etc/enable_chromium_minidumps'
        if not os.path.exists(minidumps_file):
            open(minidumps_file, 'w').close()
            # Allow browser restart by its babysitter (session_manager).
            if os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE):
                os.remove(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)
            login.nuke_login_manager()
        assert os.path.exists(minidumps_file)

        # Setup /tmp/disable_chrome_restart.
        # Disallow further browser restart by its babysitter.
        if not os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE):
            open(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE, 'w').close()
        assert os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)

    def run_once(self):
        """Runs the PyAuto performance tests."""
        # Enable Chrome testing interface and login to a default account.
        deps_dir = os.path.join(self.autodir, 'deps')
        pyautolib_dir = os.path.join(self.cr_source_dir,
                                     'chrome', 'test', 'pyautolib')
        login_cmd = cros_ui.xcommand_as(
            'python %s chromeos_utils.ChromeosUtils.LoginToDefaultAccount '
            '-v --no-http-server' %
                os.path.join(pyautolib_dir, 'chromeos', 'chromeos_utils.py'))
        utils.system(login_cmd)

        # Run the PyAuto performance tests.
        functional_cmd = cros_ui.xcommand_as(
            '%s/chrome_test/test_src/chrome/test/functional/'
            'pyauto_functional.py --suite=CHROMEOS_PERF -v' % deps_dir)
        utils.system(functional_cmd)
