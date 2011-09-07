# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pwd
import re
import shutil
import subprocess

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import constants, chrome_test, cros_ui, login


class desktopui_PyAutoPerfTests(chrome_test.ChromeTestBase):
    """Wrapper for running Chrome's PyAuto-based performance tests.

    Performs all setup and fires off the CHROMEOS_PERF PyAuto suite.
    """
    _PERF_MARKER_PRE = '_PERF_PRE_'
    _PERF_MARKER_POST = '_PERF_POST_'

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
        proc = subprocess.Popen(
            functional_cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
        output = proc.communicate()[0]
        print output  # Ensure pyauto test output is stored in autotest logs.
        if proc.returncode != 0:
          raise error.TestFail(
              'Unexpected return code from pyauto_functional.py when running '
              'with the CHROMEOS_PERF suite: %d' % proc.returncode)
        re_compiled = re.compile('%s(.+)%s' % (self._PERF_MARKER_PRE,
                                               self._PERF_MARKER_POST))
        perf_lines = [line for line in output.split('\n')
                      if re_compiled.match(line)]
        if perf_lines:
          perf_dict = dict([eval(re_compiled.match(line).group(1))
                            for line in perf_lines])
          self.write_perf_keyval(perf_dict)
