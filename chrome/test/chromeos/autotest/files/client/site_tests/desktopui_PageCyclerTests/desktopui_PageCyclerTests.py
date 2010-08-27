# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import commands, logging, os, shutil, time
from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error, site_httpd, site_ui
from page_cycler_results_parser import PageCyclerResultsParser


class desktopui_PageCyclerTests(test.test):
    version = 1
    results = {}

    def run_page_cycler(self, gtest_filter = ''):
        # TODO: Disable screensaver?
        assert(gtest_filter != ''), gtest_filter+' cannot be empty!'
        cmd = ('CR_SOURCE_ROOT=/home/chronos/chromium/src /home/chronos/'
        'chromium/src/x86-generic_out/Release/page_cycler_tests'
        ' --gtest_filter=')+gtest_filter
        xcmd = site_ui.xcommand(cmd)
        logging.debug('Running: '+gtest_filter)
        output = utils.system_output(xcmd)
        pcrp = PageCyclerResultsParser()
        result = pcrp.parse_results(output)
        logging.debug(result)
        self.results[gtest_filter] = result

    def run_once(self):
        # Use a smaller input set for testing purposes, if needed:
###        testNames=['PageCyclerTest.MozFile']
        testNames=['PageCyclerTest.Alexa_usFile', 'PageCyclerTest.MozFile',
            'PageCyclerTest.Intl1File', 'PageCyclerTest.Intl2File',
            'PageCyclerTest.DhtmlFile', 'PageCyclerTest.Moz2File',
            'PageCyclerTest.BloatFile', 'PageCyclerTest.DomFile',
            'PageCyclerTest.MorejsFile', 'PageCyclerTest.MorejsnpFile']
        for testName in testNames:
            self.run_page_cycler(testName)
        self.write_perf_keyval(self.results)
