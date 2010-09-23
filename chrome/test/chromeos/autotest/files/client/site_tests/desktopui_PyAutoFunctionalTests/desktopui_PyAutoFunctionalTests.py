# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, os, shutil, tempfile, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error, site_ui


class desktopui_PyAutoFunctionalTests(test.test):
    """Wrapper for running Chrome's PyAuto-based functional tests."""
    version = 1
    home_dir = None

    def setup(self):
        self.job.setup_dep(['chrome_test'])  # to package chrome/test/data

    def run_once(self):
        dep = 'chrome_test'
        dep_dir = os.path.join(self.autodir, 'deps', dep)
        self.job.install_pkg(dep, 'dep', dep_dir)

        cr_source_dir = '%s/test_src' % dep_dir
        test_binary_dir = '%s/test_src/out/Release' % dep_dir
        pyauto_script = '%s/test_src/chrome/test/functional/' \
                        'pyauto_functional.py' % dep_dir
        self.home_dir = tempfile.mkdtemp()

        try:
            setup_cmd = '%s/%s' % (test_binary_dir,
                                  'setup_test_links.sh')
            utils.system(setup_cmd)

            cmd = 'python %s' % pyauto_script
            cmd = 'HOME=%s CR_SOURCE_ROOT=%s %s' % (self.home_dir,
                                                    cr_source_dir,
                                                    site_ui.xcommand_as(cmd))
            logging.info("Running %s" % cmd)
            utils.system(cmd)
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('%s failed!' % pyauto_script)

    def cleanup(self):
        if self.home_dir:
            shutil.rmtree(self.home_dir, ignore_errors=True)
        test.test.cleanup(self)
