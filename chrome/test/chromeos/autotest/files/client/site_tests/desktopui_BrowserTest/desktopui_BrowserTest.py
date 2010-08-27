# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, os, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error, site_ui

class desktopui_BrowserTest(test.test):
    version = 1

    def setup(self):
        self.job.setup_dep(['chrome_test'])
        # create a empty srcdir to prevent the error that checks .version file
        if not os.path.exists(self.srcdir):
            os.mkdir(self.srcdir)


    def run_once(self):
        dep = 'chrome_test'
        dep_dir = os.path.join(self.autodir, 'deps', dep)
        self.job.install_pkg(dep, 'dep', dep_dir)

        chrome_dir = '/opt/google/chrome'
        test_binary_dir = '%s/test_src/out/Release' % dep_dir

        try:
            setup_cmd = '%s/%s' % (test_binary_dir,
                                  'setup_test_links.sh')
            utils.system(setup_cmd)

            cmd = '%s/%s' % (test_binary_dir, 'browser_tests')
            cmd = site_ui.xcommand(cmd)
            logging.info("Running %s" % cmd)
            utils.system(cmd)
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('browser_test failed.')
