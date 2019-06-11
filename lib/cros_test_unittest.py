# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for CrOSTest."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import cros_test


# pylint: disable=protected-access
class CrOSTester(cros_test_lib.RunCommandTempDirTestCase):
  """Test cros_test.CrOSTest."""

  def setUp(self):
    """Common set up method for all tests."""
    opts = cros_test.ParseCommandLine([])
    self._tester = cros_test.CrOSTest(opts)
    self._tester._device.board = 'amd64-generic'
    self._tester._device.image_path = self.TempFilePath(
        'chromiumos_qemu_image.bin')
    osutils.Touch(self._tester._device.image_path)
    version_str = ('QEMU emulator version 2.6.0, Copyright (c) '
                   '2003-2008 Fabrice Bellard')
    self.rc.AddCmdResult(partial_mock.In('--version'), output=version_str)
    self.ssh_port = self._tester._device.ssh_port

  def TempFilePath(self, file_path):
    return os.path.join(self.tempdir, file_path)

  def testBasic(self):
    """Tests basic functionality."""
    self._tester.Run()
    # Check VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])
    # Wait for VM to be responsive.
    self.assertCommandContains([
        'ssh', '-p', '9222', 'root@localhost', '--', 'true'
    ])
    # Run vm_sanity.
    self.assertCommandContains([
        'ssh', '-p', '9222', 'root@localhost', '--',
        '/usr/local/autotest/bin/vm_sanity.py'
    ])

  def testBasicAutotest(self):
    """Tests a simple autotest call."""
    self._tester.autotest = ['accessiblity_Sanity']
    self._tester.Run()

    # Check VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])

    # Checks that autotest is running.
    self.assertCommandContains([
        'test_that', '--no-quickmerge', '--ssh_options',
        '-F /dev/null -i /dev/null',
        'localhost:9222', 'accessiblity_Sanity'])

  def testSingleBaseTastTest(self):
    """Verify running a single tast test."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester.Run()
    self.assertCommandContains(['tast', 'run', '-build=false',
                                '-waituntilready', '-extrauseflags=tast_vm',
                                'localhost:9222', 'ui.ChromeLogin'])

  def testExpressionBaseTastTest(self):
    """Verify running a set of tast tests with an expression."""
    self._tester.tast = [
        '(("dep:chrome" || "dep:android") && !flaky && !disabled)'
    ]
    self._tester.Run()
    self.assertCommandContains([
        'tast', 'run', '-build=false', '-waituntilready',
        '-extrauseflags=tast_vm', 'localhost:9222',
        '(("dep:chrome" || "dep:android") && !flaky && !disabled)'
    ])

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot')
  def testTastTestWithOtherArgs(self, check_inside_chroot_mock):
    """Verify running a single tast test with various arguments."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester.test_timeout = 100
    self._tester._device.log_level = 'debug'
    self._tester._device.ssh_port = None
    self._tester._device.device = '100.90.29.199'
    self._tester.results_dir = '/tmp/results'
    self._tester.Run()
    check_inside_chroot_mock.assert_called()
    self.assertCommandContains(['tast', '-verbose', 'run', '-build=false',
                                '-waituntilready', '-timeout=100',
                                '-resultsdir', '/tmp/results', '100.90.29.199',
                                'ui.ChromeLogin'])

  def testTastTestSDK(self):
    """Verify running tast tests from the SimpleChrome SDK."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester._device.private_key = '/tmp/.ssh/testing_rsa'
    tast_cache_dir = cros_test_lib.FakeSDKCache(
        self._tester.cache_dir).CreateCacheReference(
            self._tester._device.board, 'chromeos-base')
    tast_bin_dir = os.path.join(tast_cache_dir, 'tast-cmd/usr/bin')
    osutils.SafeMakedirs(tast_bin_dir)
    self._tester.Run()
    self.assertCommandContains([
        os.path.join(tast_bin_dir, 'tast'), 'run', '-build=false',
        '-waituntilready', '-remoterunner=%s'
        % os.path.join(tast_bin_dir, 'remote_test_runner'),
        '-remotebundledir=%s' % os.path.join(tast_cache_dir,
                                             'tast-remote-tests-cros/usr',
                                             'libexec/tast/bundles/remote'),
        '-remotedatadir=%s' % os.path.join(tast_cache_dir,
                                           'tast-remote-tests-cros/usr',
                                           'share/tast/data'),
        '-ephemeraldevserver=false', '-keyfile', '/tmp/.ssh/testing_rsa',
        '-extrauseflags=tast_vm', 'localhost:9222', 'ui.ChromeLogin'
    ])
