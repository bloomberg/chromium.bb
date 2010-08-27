# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, os, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error, site_ui

class desktopui_SyncIntegrationTests(test.test):
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
        password_file = '%s/sync_password.txt' % dep_dir

        try:
            setup_cmd = '%s/%s' % (test_binary_dir,
                                  'setup_test_links.sh')
            utils.system(setup_cmd)

            cmd = '%s/sync_integration_tests --password-file-for-test=%s --test-terminate-timeout=300000' % (test_binary_dir, password_file)
            cmd = site_ui.xcommand(cmd)
            logging.info("Running %s" % cmd)
            utils.system(cmd)
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('sync_integration_tests failed.')
