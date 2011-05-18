# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import os
import shutil
import subprocess

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import constants, chrome_test, cros_ui


class desktopui_PyAutoLoginTests(chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based login tests.

    Performs all setup and fires off the login tests.
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

    def _session_manager_ready(self, old_pid):
        pgrep_process = subprocess.Popen(['pgrep', 'session_manager'],
                                        stdout=subprocess.PIPE)
        new_pid = pgrep_process.communicate()[0].strip()
        if not new_pid or old_pid == new_pid:
            return False

        try:
            bus = dbus.SystemBus()
            proxy = bus.get_object('org.chromium.SessionManager',
                                   '/org/chromium/SessionManager')
            dbus.Interface(proxy, 'org.chromium.SessionManagerInterface')
        except dbus.DBusException:
            return False
        return True

    def run_once(self):
        # Make sure Chrome minidumps are written locally.
        minidumps_file = '/mnt/stateful_partition/etc/enable_chromium_minidumps'
        if not os.path.exists(minidumps_file):
            open(minidumps_file, 'w').close()
            pgrep_process = subprocess.Popen(['pgrep', 'session_manager'],
                                            stdout=subprocess.PIPE)
            old_pid = pgrep_process.communicate()[0].strip()
            subprocess.call(['pkill', constants.SESSION_MANAGER])
            utils.poll_for_condition(
                lambda: self._session_manager_ready(old_pid), timeout=20)
        assert os.path.exists(minidumps_file)

        # Run the login pyauto tests
        deps_dir = os.path.join(self.autodir, 'deps')
        functional_cmd = cros_ui.xcommand(
            '%s/chrome_test/test_src/chrome/test/functional/'
            'chromeos_login.py -v' % deps_dir)
        utils.system(functional_cmd)
