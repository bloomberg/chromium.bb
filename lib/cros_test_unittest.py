# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for CrOSTest."""

from __future__ import print_function

import mock
import os
import stat

from chromite.lib import constants
from chromite.lib import cros_build_lib
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

  def CheckParserError(self, args, error_msg):
    """Checks that parser error is raised.

    Args:
      args: List of commandline arguments.
      error_msg: Error message to check for.
    """
    # Putting cros_build_lib.OutputCapturer() before assertRaises(SystemExit)
    # swallows SystemExit exception check.
    with self.assertRaises(SystemExit):
      with cros_build_lib.OutputCapturer() as output:
        cros_test.ParseCommandLine(args)
    self.assertIn(error_msg, output.GetStderr())

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

  def testDeployChrome(self):
    """Tests basic deploy chrome command."""
    self._tester.deploy = True
    self._tester.build_dir = self.TempFilePath('out_amd64-generic/Release')
    self._tester.Run()
    self.assertCommandContains(['deploy_chrome', '--force', '--build-dir',
                                self._tester.build_dir, '--process-timeout',
                                '180', '--to', self._tester._device.device,
                                '--port', '9222', '--board', 'amd64-generic',
                                '--cache-dir', self._tester.cache_dir])

  def testDeployChromeWithArgs(self):
    """Tests deploy chrome command with additional arguments."""
    self._tester.deploy = True
    self._tester.build_dir = self.TempFilePath('out_amd64-generic/Release')
    self._tester.nostrip = True
    self._tester.mount = True
    self._tester.Run()
    self.assertCommandContains(['--nostrip', '--mount'])

  def testCatapult(self):
    """Verify catapult test command."""
    self._tester.catapult_tests = ['testAddResults']
    self._tester.Run()
    self.assertCommandContains([
        'python', '/usr/local/telemetry/src/third_party/catapult/'
        'telemetry/bin/run_tests', '--browser=system', 'testAddResults'
    ])

  def testCatapultAsGuest(self):
    """Verify that we use the correct browser in guest mode."""
    self._tester.catapult_tests = ['testAddResults']
    self._tester.guest = True
    self._tester.Run()
    self.assertCommandContains([
        'python', '/usr/local/telemetry/src/third_party/catapult/'
        'telemetry/bin/run_tests', '--browser=system-guest', 'testAddResults'
    ])

  def testBasicAutotest(self):
    """Tests a simple autotest call."""
    self._tester.autotest = ['accessibility_Sanity']
    self._tester.Run()

    # Check VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])

    # Checks that autotest is running.
    self.assertCommandContains([
        'test_that', '--no-quickmerge', '--ssh_options',
        '-F /dev/null -i /dev/null',
        'localhost:9222', 'accessibility_Sanity'])

  def testAutotestWithArgs(self):
    """Tests an autotest call with attributes."""
    self._tester.autotest = ['accessibility_Sanity']
    self._tester.results_dir = 'test_results'
    self._tester._device.private_key = '.ssh/testing_rsa'
    self._tester._device.log_level = 'debug'
    self._tester._device.ssh_port = None
    self._tester._device.device = '100.90.29.199'
    self._tester.test_that_args = ['--test_that-args',
                                   '--whitelist-chrome-crashes']

    cwd = os.path.join('/mnt/host/source',
                       os.path.relpath(os.getcwd(), constants.SOURCE_ROOT))
    test_results_dir = os.path.join(cwd, 'test_results')
    testing_rsa_dir = os.path.join(cwd, '.ssh/testing_rsa')

    self._tester._RunAutotest()

    self.assertCommandCalled(
        ['test_that', '--board', 'amd64-generic', '--results_dir',
         test_results_dir, '--ssh_private_key', testing_rsa_dir, '--debug',
         '--whitelist-chrome-crashes', '--no-quickmerge', '--ssh_options',
         '-F /dev/null -i /dev/null', '100.90.29.199', 'accessibility_Sanity'],
        enter_chroot=not cros_build_lib.IsInsideChroot())

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testInsideChrootAutotest(self, check_inside_chroot_mock):
    """Tests running an autotest from within the chroot."""
    # Checks that mock version has been called.
    check_inside_chroot_mock.assert_called()

    self._tester.autotest = ['accessibility_Sanity']
    self._tester.results_dir = '/mnt/host/source/test_results'
    self._tester._device.private_key = '/mnt/host/source/.ssh/testing_rsa'

    self._tester._RunAutotest()

    self.assertCommandContains([
        '--results_dir', '/mnt/host/source/test_results',
        '--ssh_private_key', '/mnt/host/source/.ssh/testing_rsa'])

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideChrootAutotest(self, check_inside_chroot_mock):
    """Tests running an autotest from outside the chroot."""
    # Checks that mock version has been called.
    check_inside_chroot_mock.assert_called()

    self._tester.autotest = ['accessibility_Sanity']
    # Capture the run command. This is necessary beacuse the mock doesn't
    # capture the cros_sdk wrapper.
    with cros_build_lib.OutputCapturer() as output:
      self._tester._RunAutotest()
    # Check that we enter the chroot before running test_that.
    self.assertIn(
        'cros_sdk -- test_that --board amd64-generic --no-quickmerge'
        ' --ssh_options \'-F /dev/null -i /dev/null\' localhost:9222'
        ' accessibility_Sanity', output.GetStderr())

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

  def testRunDeviceCmd(self):
    """Verify a run device cmd call."""
    self._tester.remote_cmd = True
    self._tester.files = [self.TempFilePath('crypto_unittests')]
    osutils.Touch(self._tester.files[0], mode=stat.S_IRWXU)
    self._tester.as_chronos = True
    self._tester.args = ['crypto_unittests',
                         '--test-launcher-print-test-stdio=always']

    self._tester.Run()

    # Ensure target directory is created on the DUT.
    self.assertCommandContains(['mkdir', '-p', '/usr/local/cros_test'])
    # Ensure test ssh keys are authorized with chronos.
    self.assertCommandContains(['cp', '-r', '/root/.ssh/',
                                '/home/chronos/user/'])
    # Ensure chronos has ownership of the directory.
    self.assertCommandContains(['chown', '-R', 'chronos:',
                                '/usr/local/cros_test'])
    # Ensure command runs in the target directory.
    self.assertCommandContains('"cd /usr/local/cros_test && crypto_unittests '
                               '--test-launcher-print-test-stdio=always"')
    # Ensure target directory is removed at the end of the test.
    self.assertCommandContains(['rm', '-rf', '/usr/local/cros_test'])

  def testRunDeviceCmdWithoutSrcFiles(self):
    """Verify running a remote command when src files are not specified.

    The remote command should not change the working directory or create a temp
    directory on the target.
    """
    self._tester.remote_cmd = True
    self._tester.args = ['/usr/local/autotest/bin/vm_sanity.py']
    self._tester.Run()
    self.assertCommandContains(['ssh', '-p', '9222',
                                '/usr/local/autotest/bin/vm_sanity.py'])
    self.assertCommandContains(['mkdir', '-p'], expected=False)
    self.assertCommandContains(['"cd %s && /usr/local/autotest/bin/'
                                'vm_sanity.py"' % self._tester.cwd],
                               expected=False)
    self.assertCommandContains(['rm', '-rf'], expected=False)

  def testHostCmd(self):
    """Verify running a host command."""
    self._tester.host_cmd = True
    self._tester.args = ['tast', 'run', 'localhost:9222', 'ui.ChromeLogin']
    self._tester.Run()
    # Ensure command is run and an exception is not raised if it fails.
    self.assertCommandCalled(['tast', 'run', 'localhost:9222',
                              'ui.ChromeLogin'], error_code_ok=True)
    # Ensure that --host-cmd does not invoke ssh since it runs on the host.
    self.assertCommandContains(['ssh', 'tast'], expected=False)

  def testParserErrorChromeTest(self):
    """Verify we get a parser error for --chrome-test when no args are given."""
    self.CheckParserError(['--chrome-test'], '--chrome-test')

  def testParserSetsBuildDir(self):
    """Verify that the build directory is set when not specified."""
    test_dir = self.TempFilePath('out_amd64-generic/Release/crypto_unittests')
    # Retrieves the build directory from the parsed options.
    build_dir = cros_test.ParseCommandLine(
        ['--chrome-test', '--', test_dir]).build_dir
    self.assertEqual(build_dir, os.path.dirname(test_dir))
