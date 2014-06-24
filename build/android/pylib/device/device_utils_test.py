# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of device_utils.py (mostly DeviceUtils).
"""

# pylint: disable=C0321
# pylint: disable=W0212
# pylint: disable=W0613

import os
import sys
import unittest

from pylib import android_commands
from pylib import constants
from pylib.device import adb_wrapper
from pylib.device import device_errors
from pylib.device import device_utils

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'android_testrunner'))
import run_command as atr_run_command

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock # pylint: disable=F0401


class DeviceUtilsTest(unittest.TestCase):

  def testInitWithStr(self):
    serial_as_str = str('0123456789abcdef')
    d = device_utils.DeviceUtils('0123456789abcdef')
    self.assertEqual(serial_as_str, d.old_interface.GetDevice())

  def testInitWithUnicode(self):
    serial_as_unicode = unicode('fedcba9876543210')
    d = device_utils.DeviceUtils(serial_as_unicode)
    self.assertEqual(serial_as_unicode, d.old_interface.GetDevice())

  def testInitWithAdbWrapper(self):
    serial = '123456789abcdef0'
    a = adb_wrapper.AdbWrapper(serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.old_interface.GetDevice())

  def testInitWithAndroidCommands(self):
    serial = '0fedcba987654321'
    a = android_commands.AndroidCommands(device=serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.old_interface.GetDevice())

  def testInitWithNone(self):
    d = device_utils.DeviceUtils(None)
    self.assertIsNone(d.old_interface.GetDevice())


class DeviceUtilsOldImplTest(unittest.TestCase):

  class AndroidCommandsCalls(object):

    def __init__(self, test_case, cmd_ret):
      self._cmds = cmd_ret
      self._test_case = test_case
      self._total_received = 0

    def __enter__(self):
      atr_run_command.RunCommand = mock.Mock()
      atr_run_command.RunCommand.side_effect = lambda c, **kw: self._ret(c)

    def _ret(self, actual_cmd):
      on_failure_fmt = ('\n'
                        '  received command: %s\n'
                        '  expected command: %s')
      self._test_case.assertGreater(
          len(self._cmds), self._total_received,
          msg=on_failure_fmt % (actual_cmd, None))
      expected_cmd, ret = self._cmds[self._total_received]
      self._total_received += 1
      self._test_case.assertEqual(
          actual_cmd, expected_cmd,
          msg=on_failure_fmt % (actual_cmd, expected_cmd))
      return ret

    def __exit__(self, exc_type, _exc_val, exc_trace):
      if exc_type is None:
        on_failure = "adb commands don't match.\nExpected:%s\nActual:%s" % (
            ''.join('\n  %s' % c for c, _ in self._cmds),
            ''.join('\n  %s' % a[0]
                    for _, a, kw in atr_run_command.RunCommand.mock_calls))
        self._test_case.assertEqual(
          len(self._cmds), len(atr_run_command.RunCommand.mock_calls),
          msg=on_failure)
        for (expected_cmd, _r), (_n, actual_args, actual_kwargs) in zip(
            self._cmds, atr_run_command.RunCommand.mock_calls):
          self._test_case.assertEqual(1, len(actual_args), msg=on_failure)
          self._test_case.assertEqual(expected_cmd, actual_args[0],
                                      msg=on_failure)
          self._test_case.assertTrue('timeout_time' in actual_kwargs,
                                     msg=on_failure)
          self._test_case.assertTrue('retry_count' in actual_kwargs,
                                     msg=on_failure)

  def assertOldImplCalls(self, cmd, ret):
    return type(self).AndroidCommandsCalls(self, [(cmd, ret)])

  def assertOldImplCallsSequence(self, cmd_ret):
    return type(self).AndroidCommandsCalls(self, cmd_ret)

  def setUp(self):
    self.device = device_utils.DeviceUtils(
        '0123456789abcdef', default_timeout=1, default_retries=0)

  def testIsOnline_true(self):
    with self.assertOldImplCalls('adb -s 0123456789abcdef get-state',
                                 'device\r\n'):
      self.assertTrue(self.device.IsOnline())

  def testIsOnline_false(self):
    with self.assertOldImplCalls('adb -s 0123456789abcdef get-state', '\r\n'):
      self.assertFalse(self.device.IsOnline())

  def testHasRoot_true(self):
    with self.assertOldImplCalls("adb -s 0123456789abcdef shell 'ls /root'",
                                 'foo\r\n'):
      self.assertTrue(self.device.HasRoot())

  def testHasRoot_false(self):
    with self.assertOldImplCalls("adb -s 0123456789abcdef shell 'ls /root'",
                                 'Permission denied\r\n'):
      self.assertFalse(self.device.HasRoot())

  def testEnableRoot_succeeds(self):
    with self.assertOldImplCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type',
         'userdebug\r\n'),
        ('adb -s 0123456789abcdef root', 'restarting adbd as root\r\n'),
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef wait-for-device', '')]):
      self.device.EnableRoot()

  def testEnableRoot_userBuild(self):
    with self.assertOldImplCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type', 'user\r\n')]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()

  def testEnableRoot_rootFails(self):
    with self.assertOldImplCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type',
         'userdebug\r\n'),
        ('adb -s 0123456789abcdef root', 'no\r\n'),
        ('adb -s 0123456789abcdef wait-for-device', '')]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()

  def testGetExternalStoragePath_succeeds(self):
    fakeStoragePath = '/fake/storage/path'
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
        '%s\r\n' % fakeStoragePath):
      self.assertEquals(fakeStoragePath,
                        self.device.GetExternalStoragePath())

  def testGetExternalStoragePath_fails(self):
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'", '\r\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetExternalStoragePath()

  def testWaitUntilFullyBooted_succeedsNoWifi(self):
    with self.assertOldImplCallsSequence([
        # AndroidCommands.WaitForSystemBootCompleted
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef shell getprop sys.boot_completed', '1\r\n'),
        # AndroidCommands.WaitForDevicePm
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef shell pm path android',
         'package:this.is.a.test.package'),
        # AndroidCommands.WaitForSdCardReady
        ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
         '/fake/storage/path'),
        ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'",
         'nothing\r\n')
        ]):
      self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_succeedsWithWifi(self):
    with self.assertOldImplCallsSequence([
        # AndroidCommands.WaitForSystemBootCompleted
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef shell getprop sys.boot_completed', '1\r\n'),
        # AndroidCommands.WaitForDevicePm
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef shell pm path android',
         'package:this.is.a.test.package'),
        # AndroidCommands.WaitForSdCardReady
        ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
         '/fake/storage/path'),
        ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'",
         'nothing\r\n'),
        # wait for wifi
        ("adb -s 0123456789abcdef shell 'dumpsys wifi'", 'Wi-Fi is enabled')]):
      self.device.WaitUntilFullyBooted(wifi=True)

  def testWaitUntilFullyBooted_bootFails(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
          # AndroidCommands.WaitForSystemBootCompleted
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell getprop sys.boot_completed',
           '0\r\n')]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_devicePmFails(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
          # AndroidCommands.WaitForSystemBootCompleted
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell getprop sys.boot_completed',
           '1\r\n')]
          # AndroidCommands.WaitForDevicePm
        + 3 * ([('adb -s 0123456789abcdef wait-for-device', '')]
             + 24 * [('adb -s 0123456789abcdef shell pm path android', '\r\n')]
             + [("adb -s 0123456789abcdef shell 'stop'", '\r\n'),
                ("adb -s 0123456789abcdef shell 'start'", '\r\n')])):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_sdCardReadyFails_noPath(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
          # AndroidCommands.WaitForSystemBootCompleted
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell getprop sys.boot_completed',
           '1\r\n'),
          # AndroidCommands.WaitForDevicePm
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell pm path android',
           'package:this.is.a.test.package'),
          ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'", '\r\n')]):
        with self.assertRaises(device_errors.CommandFailedError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_sdCardReadyFails_emptyPath(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
          # AndroidCommands.WaitForSystemBootCompleted
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell getprop sys.boot_completed',
           '1\r\n'),
          # AndroidCommands.WaitForDevicePm
          ('adb -s 0123456789abcdef wait-for-device', ''),
          ('adb -s 0123456789abcdef shell pm path android',
           'package:this.is.a.test.package'),
          ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
           '/fake/storage/path\r\n'),
          ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'", '')]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testReboot_nonBlocking(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
            ('adb -s 0123456789abcdef reboot', ''),
            ('adb -s 0123456789abcdef get-state', 'unknown\r\n'),
            ('adb -s 0123456789abcdef wait-for-device', ''),
            ('adb -s 0123456789abcdef shell pm path android',
             'package:this.is.a.test.package'),
            ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
             '/fake/storage/path\r\n'),
            ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'",
             'nothing\r\n')]):
        self.device.Reboot(block=False)

  def testReboot_blocking(self):
    with mock.patch('time.sleep'):
      with self.assertOldImplCallsSequence([
            ('adb -s 0123456789abcdef reboot', ''),
            ('adb -s 0123456789abcdef get-state', 'unknown\r\n'),
            ('adb -s 0123456789abcdef wait-for-device', ''),
            ('adb -s 0123456789abcdef shell pm path android',
             'package:this.is.a.test.package'),
            ("adb -s 0123456789abcdef shell 'echo $EXTERNAL_STORAGE'",
             '/fake/storage/path\r\n'),
            ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'",
             'nothing\r\n'),
            ('adb -s 0123456789abcdef wait-for-device', ''),
            ('adb -s 0123456789abcdef shell getprop sys.boot_completed',
             '1\r\n'),
            ('adb -s 0123456789abcdef wait-for-device', ''),
            ('adb -s 0123456789abcdef shell pm path android',
             'package:this.is.a.test.package'),
            ("adb -s 0123456789abcdef shell 'ls /fake/storage/path'",
             'nothing\r\n')]):
        self.device.Reboot(block=True)

  def testInstall_noPriorInstall(self):
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')):
      with self.assertOldImplCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           ''),
          ("adb -s 0123456789abcdef install /fake/test/app.apk",
           'Success\r\n')]):
        self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_differentPriorInstall(self):
    def mockGetFilesChanged(host_path, device_path, ignore_filenames):
      return [(host_path, device_path)]

    # Pylint raises a false positive "operator not preceded by a space"
    # warning below.
    # pylint: disable=C0322
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('os.path.exists', return_value=True)), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')), (
         mock.patch('pylib.constants.GetOutDirectory',
                    return_value='/fake/test/out')), (
         mock.patch('pylib.android_commands.AndroidCommands.GetFilesChanged',
                    side_effect=mockGetFilesChanged)):
    # pylint: enable=C0322
      with self.assertOldImplCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           'package:/fake/data/app/this.is.a.test.package.apk\r\n'),
          # GetFilesChanged is mocked, so its adb calls are omitted.
          ('adb -s 0123456789abcdef uninstall this.is.a.test.package',
           'Success\r\n'),
          ('adb -s 0123456789abcdef install /fake/test/app.apk',
           'Success\r\n')]):
        self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_differentPriorInstall_reinstall(self):
    def mockGetFilesChanged(host_path, device_path, ignore_filenames):
      return [(host_path, device_path)]

    # Pylint raises a false positive "operator not preceded by a space"
    # warning below.
    # pylint: disable=C0322
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')), (
         mock.patch('pylib.constants.GetOutDirectory',
                    return_value='/fake/test/out')), (
         mock.patch('pylib.android_commands.AndroidCommands.GetFilesChanged',
                    side_effect=mockGetFilesChanged)):
    # pylint: enable=C0322
      with self.assertOldImplCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           'package:/fake/data/app/this.is.a.test.package.apk\r\n'),
          # GetFilesChanged is mocked, so its adb calls are omitted.
          ('adb -s 0123456789abcdef install -r /fake/test/app.apk',
           'Success\r\n')]):
        self.device.Install('/fake/test/app.apk', reinstall=True, retries=0)

  def testInstall_identicalPriorInstall(self):
    def mockGetFilesChanged(host_path, device_path, ignore_filenames):
      return []

    with mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package'), (
         mock.patch('pylib.android_commands.AndroidCommands.GetFilesChanged',
                    side_effect=mockGetFilesChanged)):
      with self.assertOldImplCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           'package:/fake/data/app/this.is.a.test.package.apk\r\n')
          # GetFilesChanged is mocked, so its adb calls are omitted.
          ]):
        self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_fails(self):
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')):
      with self.assertOldImplCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           ''),
          ("adb -s 0123456789abcdef install /fake/test/app.apk",
           'Failure\r\n')]):
        with self.assertRaises(device_errors.CommandFailedError):
          self.device.Install('/fake/test/app.apk', retries=0)

  def testRunShellCommand_commandAsList(self):
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'pm list packages'",
        'pacakge:android\r\n'):
      self.device.RunShellCommand(['pm', 'list', 'packages'])

  def testRunShellCommand_commandAsString(self):
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'dumpsys wifi'",
        'Wi-Fi is enabled\r\n'):
      self.device.RunShellCommand('dumpsys wifi')

  def testRunShellCommand_withSu(self):
    with self.assertOldImplCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls /root'", 'Permission denied\r\n'),
        ("adb -s 0123456789abcdef shell 'su -c setprop service.adb.root 0'",
         '')]):
      self.device.RunShellCommand('setprop service.adb.root 0', root=True)

  def testRunShellCommand_withRoot(self):
    with self.assertOldImplCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls /root'", 'hello\r\nworld\r\n'),
        ("adb -s 0123456789abcdef shell 'setprop service.adb.root 0'", '')]):
      self.device.RunShellCommand('setprop service.adb.root 0', root=True)

  def testRunShellCommand_checkReturn_success(self):
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'echo $ANDROID_DATA; echo %$?'",
        '/data\r\n%0\r\n'):
      self.device.RunShellCommand('echo $ANDROID_DATA', check_return=True)

  def testRunShellCommand_checkReturn_failure(self):
    with self.assertOldImplCalls(
        "adb -s 0123456789abcdef shell 'echo $ANDROID_DATA; echo %$?'",
        '\r\n%1\r\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.RunShellCommand('echo $ANDROID_DATA', check_return=True)

if __name__ == '__main__':
  unittest.main(verbosity=2)

