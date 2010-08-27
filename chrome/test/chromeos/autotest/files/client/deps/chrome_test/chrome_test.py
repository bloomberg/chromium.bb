#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common, commands, logging, os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    if 'CHROME_ORIGIN' not in os.environ.keys() or \
       (os.environ['CHROME_ORIGIN'] != 'LOCAL_SOURCE' and
        os.environ['CHROME_ORIGIN'] != 'SERVER_SOURCE'):
        logging.info('Skipping Chrome test resource setup for non-local builds')
        return

    chrome_test_files = os.environ['SYSROOT'] + '/usr/local/autotest-chrome'
    logging.info('Configuring chrome test resources in %s' % top_dir)
    testsrc_dir = top_dir + '/test_src'

    # Copy test build outputs.
    utils.run('cp -ral %s %s' % (chrome_test_files, testsrc_dir))


pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, pwd)
