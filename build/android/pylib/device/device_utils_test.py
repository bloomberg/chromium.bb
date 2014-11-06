#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of device_utils.py (mostly DeviceUtils).
"""

# pylint: disable=C0321
# pylint: disable=W0212
# pylint: disable=W0613

import collections
import datetime
import logging
import os
import re
import signal
import sys
import unittest

from pylib import android_commands
from pylib import constants
from pylib.device import adb_wrapper
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.device import intent

# RunCommand from third_party/android_testrunner/run_command.py is mocked
# below, so its path needs to be in sys.path.
sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'android_testrunner'))

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


class _PatchedFunction(object):
  def __init__(self, patched=None, mocked=None):
    self.patched = patched
    self.mocked = mocked


class MockFileSystem(object):

  @staticmethod
  def osStatResult(
      st_mode=None, st_ino=None, st_dev=None, st_nlink=None, st_uid=None,
      st_gid=None, st_size=None, st_atime=None, st_mtime=None, st_ctime=None):
    MockOSStatResult = collections.namedtuple('MockOSStatResult', [
        'st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid',
        'st_size', 'st_atime', 'st_mtime', 'st_ctime'])
    return MockOSStatResult(st_mode, st_ino, st_dev, st_nlink, st_uid, st_gid,
                            st_size, st_atime, st_mtime, st_ctime)

  MOCKED_FUNCTIONS = [
    ('os.listdir', []),
    ('os.path.abspath', ''),
    ('os.path.dirname', ''),
    ('os.path.exists', False),
    ('os.path.getsize', 0),
    ('os.path.isdir', False),
    ('os.stat', osStatResult.__func__()),
    ('os.walk', []),
  ]

  def _get(self, mocked, path, default_val):
    if self._verbose:
      logging.debug('%s(%s)' % (mocked, path))
    return (self.mock_file_info[path][mocked]
            if path in self.mock_file_info
            else default_val)

  def _patched(self, target, default_val=None):
    r = lambda f: self._get(target, f, default_val)
    return _PatchedFunction(patched=mock.patch(target, side_effect=r))

  def __init__(self, verbose=False):
    self.mock_file_info = {}
    self._patched_functions = [
        self._patched(m, d) for m, d in type(self).MOCKED_FUNCTIONS]
    self._verbose = verbose

  def addMockFile(self, path, **kw):
    self._addMockThing(path, False, **kw)

  def addMockDirectory(self, path, **kw):
    self._addMockThing(path, True, **kw)

  def _addMockThing(self, path, is_dir, listdir=None, size=0, stat=None,
                    walk=None):
    if listdir is None:
      listdir = []
    if stat is None:
      stat = self.osStatResult()
    if walk is None:
      walk = []

    dirname = os.sep.join(path.rstrip(os.sep).split(os.sep)[:-1])
    if dirname and not dirname in self.mock_file_info:
      self._addMockThing(dirname, True)

    self.mock_file_info[path] = {
      'os.listdir': listdir,
      'os.path.abspath': path,
      'os.path.dirname': dirname,
      'os.path.exists': True,
      'os.path.isdir': is_dir,
      'os.path.getsize': size,
      'os.stat': stat,
      'os.walk': walk,
    }

  def __enter__(self):
    for p in self._patched_functions:
      p.mocked = p.patched.__enter__()

  def __exit__(self, exc_type, exc_val, exc_tb):
    for p in self._patched_functions:
      p.patched.__exit__()


class DeviceUtilsOldImplTest(unittest.TestCase):

  class AndroidCommandsCalls(object):

    def __init__(self, test_case, cmd_ret, comp):
      self._cmds = cmd_ret
      self._comp = comp
      self._run_command = _PatchedFunction()
      self._test_case = test_case
      self._total_received = 0

    def __enter__(self):
      self._run_command.patched = mock.patch(
          'run_command.RunCommand',
          side_effect=lambda c, **kw: self._ret(c))
      self._run_command.mocked = self._run_command.patched.__enter__()

    def _ret(self, actual_cmd):
      if sys.exc_info()[0] is None:
        on_failure_fmt = ('\n'
                          '  received command: %s\n'
                          '  expected command: %s')
        self._test_case.assertGreater(
            len(self._cmds), self._total_received,
            msg=on_failure_fmt % (actual_cmd, None))
        expected_cmd, ret = self._cmds[self._total_received]
        self._total_received += 1
        self._test_case.assertTrue(
            self._comp(expected_cmd, actual_cmd),
            msg=on_failure_fmt % (actual_cmd, expected_cmd))
        return ret
      return ''

    def __exit__(self, exc_type, exc_val, exc_tb):
      self._run_command.patched.__exit__(exc_type, exc_val, exc_tb)
      if exc_type is None:
        on_failure = "adb commands don't match.\nExpected:%s\nActual:%s" % (
            ''.join('\n  %s' % c for c, _ in self._cmds),
            ''.join('\n  %s' % a[0]
                    for _, a, kw in self._run_command.mocked.mock_calls))
        self._test_case.assertEqual(
          len(self._cmds), len(self._run_command.mocked.mock_calls),
          msg=on_failure)
        for (expected_cmd, _r), (_n, actual_args, actual_kwargs) in zip(
            self._cmds, self._run_command.mocked.mock_calls):
          self._test_case.assertEqual(1, len(actual_args), msg=on_failure)
          self._test_case.assertTrue(self._comp(expected_cmd, actual_args[0]),
                                     msg=on_failure)
          self._test_case.assertTrue('timeout_time' in actual_kwargs,
                                     msg=on_failure)
          self._test_case.assertTrue('retry_count' in actual_kwargs,
                                     msg=on_failure)

  def assertNoAdbCalls(self):
    return type(self).AndroidCommandsCalls(self, [], str.__eq__)

  def assertCalls(self, cmd, ret, comp=str.__eq__):
    return type(self).AndroidCommandsCalls(self, [(cmd, ret)], comp)

  def assertCallsSequence(self, cmd_ret, comp=str.__eq__):
    return type(self).AndroidCommandsCalls(self, cmd_ret, comp)

  def setUp(self):
    self.device = device_utils.DeviceUtils(
        '0123456789abcdef', default_timeout=1, default_retries=0)


class Args:
  def __init__(self, *args, **kwargs):
    self.args = args
    self.kwargs = kwargs

  def __eq__(self, other):
    return (self.args, self.kwargs) == (other.args, other.kwargs)

  def __repr__(self):
    return '%s(%s)' % (type(self).__name__, str(self))

  def __str__(self):
    toks = (['%r' % v for v in self.args] +
            ['%s=%r' % (k, self.kwargs[k]) for k in sorted(self.kwargs)])
    return ', '.join(toks)


class MockCallSequence(object):
  def __init__(self, test_case, obj, method, calls):
    def assert_and_return(*args, **kwargs):
      received_args = Args(*args, **kwargs)
      test_case.assertTrue(
          self._calls,
          msg=('Unexpected call\n'
               '  received: %s(%s)\n' % (self._method, received_args)))
      expected_args, return_value = self._calls.pop(0)
      test_case.assertTrue(
          received_args == expected_args,
          msg=('Call does not match expected args\n'
               '  received: %s(%s)\n'
               '  expected: %s(%s)\n'
               % (self._method, received_args,
                  self._method, expected_args)))
      if isinstance(return_value, Exception):
        raise return_value
      else:
        return return_value

    self._calls = list(calls)
    self._test_case = test_case
    self._method = method
    self._patched = mock.patch.object(obj, self._method,
                                      side_effect=assert_and_return)

  def __enter__(self):
    return self._patched.__enter__()

  def __exit__(self, exc_type, exc_val, exc_tb):
    self._patched.__exit__(exc_type, exc_val, exc_tb)
    if exc_type is None:
      missing = ''.join('  expected: %s(%s)\n'
                        % (self._method, expected_args)
                        for expected_args, _ in self._calls)
      self._test_case.assertTrue(
          not missing,
          msg=('Expected calls not found\n' + missing))


class _ShellError:
  def __init__(self, output=None, return_code=1):
    if output is None:
      self.output = 'Permission denied\r\n'
    else:
      self.output = output
    self.return_code = return_code


class _CmdTimeout:
  def __init__(self, msg=None):
    if msg is None:
      self.msg = 'Operation timed out'
    else:
      self.msg = msg


class DeviceUtilsNewImplTest(unittest.TestCase):

  def setUp(self):
    test_serial = '0123456789abcdef'
    self.adb = mock.Mock(spec=adb_wrapper.AdbWrapper)
    self.adb.__str__ = mock.Mock(return_value=test_serial)
    self.adb.GetDeviceSerial.return_value = test_serial
    self.device = device_utils.DeviceUtils(
        self.adb, default_timeout=10, default_retries=0)

  def assertShellCallSequence(self, calls):
    '''Assert that we expect a sequence of calls to adb.Shell.

    Args:
      calls: a sequence of (cmd, return_value) pairs, where |cmd| is the
        expected shell command to run on the device (with any quoting already
        applied), and |return_value| is either a string to give as mock output
        or a _ShellError object to raise an AdbShellCommandFailedError.
    '''
    def mk_expected_call(cmd, return_value):
      expected_args = Args(cmd, expect_rc=0)
      if isinstance(return_value, _ShellError):
        return_value = device_errors.AdbShellCommandFailedError(cmd,
            return_value.return_code, return_value.output, str(self.device))
      elif isinstance(return_value, _CmdTimeout):
        return_value = device_errors.CommandTimeoutError(return_value.msg,
                                                         str(self.device))
      return (expected_args, return_value)

    expected_calls = (mk_expected_call(a, r) for a, r in calls)
    return MockCallSequence(self, self.adb, 'Shell', expected_calls)

  def assertShellCall(self, cmd, return_value=''):
    return self.assertShellCallSequence([(cmd, return_value)])


class DeviceUtilsHybridImplTest(DeviceUtilsOldImplTest):

  def setUp(self):
    super(DeviceUtilsHybridImplTest, self).setUp()
    self.device.adb = self.adb = mock.Mock(spec=adb_wrapper.AdbWrapper)


class DeviceUtilsIsOnlineTest(DeviceUtilsNewImplTest):

  def testIsOnline_true(self):
    self.adb.GetState = mock.Mock(return_value='device')
    self.assertTrue(self.device.IsOnline())
    self.adb.GetState.assert_called_once_with()

  def testIsOnline_false(self):
    self.adb.GetState = mock.Mock(return_value='offline')
    self.assertFalse(self.device.IsOnline())
    self.adb.GetState.assert_called_once_with()

  def testIsOnline_error(self):
    self.adb.GetState = mock.Mock(
     side_effect=device_errors.CommandFailedError('falied'))
    self.assertFalse(self.device.IsOnline())
    self.adb.GetState.assert_called_once_with()

class DeviceUtilsHasRootTest(DeviceUtilsNewImplTest):

  def testHasRoot_true(self):
    with self.assertShellCall('ls /root', 'foo\r\n'):
      self.assertTrue(self.device.HasRoot())

  def testHasRoot_false(self):
    with self.assertShellCall('ls /root', _ShellError()):
      self.assertFalse(self.device.HasRoot())


class DeviceUtilsEnableRootTest(DeviceUtilsOldImplTest):

  def testEnableRoot_succeeds(self):
    with self.assertCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type',
         'userdebug\r\n'),
        ('adb -s 0123456789abcdef root', 'restarting adbd as root\r\n'),
        ('adb -s 0123456789abcdef wait-for-device', ''),
        ('adb -s 0123456789abcdef wait-for-device', '')]):
      self.device.EnableRoot()

  def testEnableRoot_userBuild(self):
    with self.assertCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type', 'user\r\n')]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()

  def testEnableRoot_rootFails(self):
    with self.assertCallsSequence([
        ('adb -s 0123456789abcdef shell getprop ro.build.type',
         'userdebug\r\n'),
        ('adb -s 0123456789abcdef root', 'no\r\n'),
        ('adb -s 0123456789abcdef wait-for-device', '')]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()


class DeviceUtilsIsUserBuildTest(DeviceUtilsNewImplTest):

  def testIsUserBuild_yes(self):
    with self.assertShellCall('getprop ro.build.type', 'user\r\n'):
      self.assertTrue(self.device.IsUserBuild())

  def testIsUserBuild_no(self):
    with self.assertShellCall('getprop ro.build.type', 'userdebug\r\n'):
      self.assertFalse(self.device.IsUserBuild())


class DeviceUtilsGetExternalStoragePathTest(DeviceUtilsNewImplTest):

  def testGetExternalStoragePath_succeeds(self):
    fakeStoragePath = '/fake/storage/path'
    with self.assertShellCall('echo $EXTERNAL_STORAGE',
                              '%s\r\n' % fakeStoragePath):
      self.assertEquals(fakeStoragePath,
                        self.device.GetExternalStoragePath())

  def testGetExternalStoragePath_fails(self):
    with self.assertShellCall('echo $EXTERNAL_STORAGE', '\r\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetExternalStoragePath()


class DeviceUtilsGetApplicationPathTest(DeviceUtilsNewImplTest):

  def testGetApplicationPath_exists(self):
    with self.assertShellCall('pm path android',
                              'package:/path/to/android.apk\n'):
      self.assertEquals('/path/to/android.apk',
                        self.device.GetApplicationPath('android'))

  def testGetApplicationPath_notExists(self):
    with self.assertShellCall('pm path not.installed.app',
                              ''):
      self.assertEquals(None,
                        self.device.GetApplicationPath('not.installed.app'))

  def testGetApplicationPath_fails(self):
    with self.assertShellCall('pm path android',
                              'ERROR. Is package manager running?\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetApplicationPath('android')


class DeviceUtilsWaitUntilFullyBootedTest(DeviceUtilsNewImplTest):

  def testWaitUntilFullyBooted_succeedsNoWifi(self):
    with self.assertShellCallSequence([
        # sc_card_ready
        ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
        ('test -d /fake/storage/path', ''),
        # pm_ready
        ('pm path android', 'package:this.is.a.test.package\r\n'),
        # boot_completed
        ('getprop sys.boot_completed', '1\r\n')]):
      self.device.WaitUntilFullyBooted(wifi=False)
      self.adb.WaitForDevice.assert_called_once_with()

  def testWaitUntilFullyBooted_succeedsWithWifi(self):
    with self.assertShellCallSequence([
        # sc_card_ready
        ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
        ('test -d /fake/storage/path', ''),
        # pm_ready
        ('pm path android', 'package:this.is.a.test.package\r\n'),
        # boot_completed
        ('getprop sys.boot_completed', '1\r\n'),
        # wifi_enabled
        ('dumpsys wifi', 'stuff\r\nWi-Fi is enabled\r\nmore stuff\r\n')]):
      self.device.WaitUntilFullyBooted(wifi=True)
      self.adb.WaitForDevice.assert_called_once_with()

  def testWaitUntilFullyBooted_sdCardReadyFails_noPath(self):
    with self.assertShellCallSequence([
        # sc_card_ready
        ('echo $EXTERNAL_STORAGE', '\r\n')]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_sdCardReadyFails_emptyPath(self):
    with mock.patch('time.sleep'):
      with self.assertShellCallSequence([
          # sc_card_ready
          ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
          ('test -d /fake/storage/path', _ShellError()),
          # sc_card_ready
          ('test -d /fake/storage/path', _ShellError()),
          # sc_card_ready
          ('test -d /fake/storage/path', _CmdTimeout())]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_devicePmFails(self):
    with mock.patch('time.sleep'):
      with self.assertShellCallSequence([
          # sc_card_ready
          ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
          ('test -d /fake/storage/path', ''),
          # pm_ready
          ('pm path android', 'Error. Is package manager running?\r\n'),
          # pm_ready
          ('pm path android', 'Error. Is package manager running?\r\n'),
          # pm_ready
          ('pm path android', _CmdTimeout())]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_bootFails(self):
    with mock.patch('time.sleep'):
      with self.assertShellCallSequence([
          # sc_card_ready
          ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
          ('test -d /fake/storage/path', ''),
          # pm_ready
          ('pm path android', 'package:this.is.a.test.package\r\n'),
          # boot_completed
          ('getprop sys.boot_completed', '0\r\n'),
          # boot_completed
          ('getprop sys.boot_completed', '0\r\n'),
          # boot_completed
          ('getprop sys.boot_completed', _CmdTimeout())]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_wifiFails(self):
    with mock.patch('time.sleep'):
      with self.assertShellCallSequence([
          # sc_card_ready
          ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
          ('test -d /fake/storage/path', ''),
          # pm_ready
          ('pm path android', 'package:this.is.a.test.package\r\n'),
          # boot_completed
          ('getprop sys.boot_completed', '1\r\n'),
          # wifi_enabled
          ('dumpsys wifi', 'stuff\r\nmore stuff\r\n'),
          # wifi_enabled
          ('dumpsys wifi', 'stuff\r\nmore stuff\r\n'),
          # wifi_enabled
          ('dumpsys wifi', _CmdTimeout())]):
        with self.assertRaises(device_errors.CommandTimeoutError):
          self.device.WaitUntilFullyBooted(wifi=True)


class DeviceUtilsRebootTest(DeviceUtilsNewImplTest):

  def testReboot_nonBlocking(self):
    self.adb.Reboot = mock.Mock()
    self.device.IsOnline = mock.Mock(return_value=False)
    self.device.Reboot(block=False)
    self.adb.Reboot.assert_called_once_with()
    self.device.IsOnline.assert_called_once_with()

  def testReboot_blocking(self):
    self.adb.Reboot = mock.Mock()
    self.device.IsOnline = mock.Mock(return_value=False)
    with self.assertShellCallSequence([
        # sc_card_ready
        ('echo $EXTERNAL_STORAGE', '/fake/storage/path\r\n'),
        ('test -d /fake/storage/path', ''),
        # pm_ready
        ('pm path android', 'package:this.is.a.test.package\r\n'),
        # boot_completed
        ('getprop sys.boot_completed', '1\r\n')]):
      self.device.Reboot(block=True)
      self.adb.Reboot.assert_called_once_with()
      self.device.IsOnline.assert_called_once_with()
      self.adb.WaitForDevice.assert_called_once_with()


class DeviceUtilsInstallTest(DeviceUtilsOldImplTest):

  def testInstall_noPriorInstall(self):
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')):
      with self.assertCallsSequence([
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
      with self.assertCallsSequence([
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
      with self.assertCallsSequence([
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
      with self.assertCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           'package:/fake/data/app/this.is.a.test.package.apk\r\n')
          # GetFilesChanged is mocked, so its adb calls are omitted.
          ]):
        self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_fails(self):
    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')):
      with self.assertCallsSequence([
          ("adb -s 0123456789abcdef shell 'pm path this.is.a.test.package'",
           ''),
          ("adb -s 0123456789abcdef install /fake/test/app.apk",
           'Failure\r\n')]):
        with self.assertRaises(device_errors.CommandFailedError):
          self.device.Install('/fake/test/app.apk', retries=0)


class DeviceUtilsRunShellCommandTest(DeviceUtilsNewImplTest):
  def testRunShellCommand_commandAsList(self):
    with self.assertShellCall('pm list packages'):
      self.device.RunShellCommand(['pm', 'list', 'packages'])

  def testRunShellCommand_commandAsListQuoted(self):
    with self.assertShellCall("echo 'hello world' '$10'"):
      self.device.RunShellCommand(['echo', 'hello world', '$10'])

  def testRunShellCommand_commandAsString(self):
    with self.assertShellCall('echo "$VAR"'):
      self.device.RunShellCommand('echo "$VAR"')

  def testNewRunShellImpl_withEnv(self):
    with self.assertShellCall('VAR=some_string echo "$VAR"'):
      self.device.RunShellCommand('echo "$VAR"', env={'VAR': 'some_string'})

  def testNewRunShellImpl_withEnvQuoted(self):
    with self.assertShellCall('PATH="$PATH:/other/path" run_this'):
      self.device.RunShellCommand('run_this', env={'PATH': '$PATH:/other/path'})

  def testNewRunShellImpl_withEnv_failure(self):
    with self.assertRaises(KeyError):
      self.device.RunShellCommand('some_cmd', env={'INVALID NAME': 'value'})

  def testNewRunShellImpl_withCwd(self):
    with self.assertShellCall('cd /some/test/path && ls'):
      self.device.RunShellCommand('ls', cwd='/some/test/path')

  def testNewRunShellImpl_withCwdQuoted(self):
    with self.assertShellCall("cd '/some test/path with/spaces' && ls"):
      self.device.RunShellCommand('ls', cwd='/some test/path with/spaces')

  def testRunShellCommand_withSu(self):
    with self.assertShellCallSequence([
        ('su -c ls /root && ! ls /root', ''),
        ('su -c setprop service.adb.root 0', '')]):
      self.device.RunShellCommand('setprop service.adb.root 0', as_root=True)

  def testRunShellCommand_withRoot(self):
    with self.assertShellCallSequence([
        ('su -c ls /root && ! ls /root', _ShellError()),
        ('setprop service.adb.root 0', '')]):
      self.device.RunShellCommand('setprop service.adb.root 0', as_root=True)

  def testRunShellCommand_manyLines(self):
    cmd = 'ls /some/path'
    with self.assertShellCall(cmd, 'file1\r\nfile2\r\nfile3\r\n'):
      self.assertEquals(['file1', 'file2', 'file3'],
                        self.device.RunShellCommand(cmd))

  def testRunShellCommand_singleLine_success(self):
    cmd = 'echo $VALUE'
    with self.assertShellCall(cmd, 'some value\r\n'):
      self.assertEquals('some value',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successEmptyLine(self):
    cmd = 'echo $VALUE'
    with self.assertShellCall(cmd, '\r\n'):
      self.assertEquals('',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successWithoutEndLine(self):
    cmd = 'echo -n $VALUE'
    with self.assertShellCall(cmd, 'some value'):
      self.assertEquals('some value',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successNoOutput(self):
    cmd = 'echo -n $VALUE'
    with self.assertShellCall(cmd, ''):
      self.assertEquals('',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_failTooManyLines(self):
    cmd = 'echo $VALUE'
    with self.assertShellCall(cmd, 'some value\r\nanother value\r\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.RunShellCommand(cmd, single_line=True)

  def testRunShellCommand_checkReturn_success(self):
    cmd = 'echo $ANDROID_DATA'
    output = '/data\r\n'
    with self.assertShellCall(cmd, output):
      self.assertEquals([output.rstrip()],
                        self.device.RunShellCommand(cmd, check_return=True))

  def testRunShellCommand_checkReturn_failure(self):
    cmd = 'ls /root'
    output = 'opendir failed, Permission denied\r\n'
    with self.assertShellCall(cmd, _ShellError(output)):
      with self.assertRaises(device_errors.AdbShellCommandFailedError):
        self.device.RunShellCommand(cmd, check_return=True)

  def testRunShellCommand_checkReturn_disabled(self):
    cmd = 'ls /root'
    output = 'opendir failed, Permission denied\r\n'
    with self.assertShellCall(cmd, _ShellError(output)):
      self.assertEquals([output.rstrip()],
                        self.device.RunShellCommand(cmd, check_return=False))


class DeviceUtilsKillAllTest(DeviceUtilsNewImplTest):

  def testKillAll_noMatchingProcesses(self):
    output = 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
    with self.assertShellCallSequence([('ps', output)]):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.KillAll('test_process')

  def testKillAll_nonblocking(self):
    with self.assertShellCallSequence([
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
               'u0_a1  1234  174   123456 54321 ffffffff 456789ab '
               'this.is.a.test.process\r\n'),
        ('kill -9 1234', '')]):
      self.assertEquals(1,
          self.device.KillAll('this.is.a.test.process', blocking=False))

  def testKillAll_blocking(self):
    with mock.patch('time.sleep'):
      with self.assertShellCallSequence([
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
               'u0_a1  1234  174   123456 54321 ffffffff 456789ab '
               'this.is.a.test.process\r\n'),
        ('kill -9 1234', ''),
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
               'u0_a1  1234  174   123456 54321 ffffffff 456789ab '
               'this.is.a.test.process\r\n'),
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n')]):
        self.assertEquals(1,
            self.device.KillAll('this.is.a.test.process', blocking=True))

  def testKillAll_root(self):
    with self.assertShellCallSequence([
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
               'u0_a1  1234  174   123456 54321 ffffffff 456789ab '
               'this.is.a.test.process\r\n'),
        ('su -c ls /root && ! ls /root', ''),
        ('su -c kill -9 1234', '')]):
      self.assertEquals(1,
          self.device.KillAll('this.is.a.test.process', as_root=True))

  def testKillAll_sigterm(self):
    with self.assertShellCallSequence([
        ('ps', 'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
               'u0_a1  1234  174   123456 54321 ffffffff 456789ab '
               'this.is.a.test.process\r\n'),
        ('kill -15 1234', '')]):
      self.assertEquals(1,
          self.device.KillAll('this.is.a.test.process', signum=signal.SIGTERM))


class DeviceUtilsStartActivityTest(DeviceUtilsOldImplTest):

  def testStartActivity_actionOnly(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_success(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_failure(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main'",
        'Error: Failed to start test activity'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.StartActivity(test_intent)

  def testStartActivity_blocking(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-W "
            "-n this.is.a.test.package/.Main'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent, blocking=True)

  def testStartActivity_withCategory(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                category='android.intent.category.HOME')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-c android.intent.category.HOME "
            "-n this.is.a.test.package/.Main'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withMultipleCategories(self):
    # The new implementation will start the activity with all provided
    # categories. The old one only uses the first category.
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                category=['android.intent.category.HOME',
                                          'android.intent.category.BROWSABLE'])
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-c android.intent.category.HOME "
            "-n this.is.a.test.package/.Main'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withData(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                data='http://www.google.com/')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "-d \"http://www.google.com/\"'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withStringExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': 'test'})
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "--es foo test'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withBoolExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': True})
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "--ez foo True'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withIntExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': 123})
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "--ei foo 123'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withTraceFile(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "--start-profiler test_trace_file.out'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent,
                                trace_file_name='test_trace_file.out')

  def testStartActivity_withForceStop(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-S "
            "-n this.is.a.test.package/.Main'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent, force_stop=True)

  def testStartActivity_withFlags(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                flags='0x10000000')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-a android.intent.action.VIEW "
            "-n this.is.a.test.package/.Main "
            "-f 0x10000000'",
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)


class DeviceUtilsBroadcastIntentTest(DeviceUtilsOldImplTest):

  def testBroadcastIntent_noExtras(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT')
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am broadcast "
            "-a test.package.with.an.INTENT '",
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)

  def testBroadcastIntent_withExtra(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT',
                                extras={'foo': 'bar'})
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am broadcast "
            "-a test.package.with.an.INTENT "
            "-e foo \"bar\"'",
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)

  def testBroadcastIntent_withExtra_noValue(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT',
                                extras={'foo': None})
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am broadcast "
            "-a test.package.with.an.INTENT "
            "-e foo'",
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)


class DeviceUtilsGoHomeTest(DeviceUtilsOldImplTest):

  def testGoHome(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am start "
            "-W "
            "-a android.intent.action.MAIN "
            "-c android.intent.category.HOME'",
        'Starting: Intent { act=android.intent.action.MAIN }\r\n'):
      self.device.GoHome()


class DeviceUtilsForceStopTest(DeviceUtilsOldImplTest):

  def testForceStop(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'am force-stop this.is.a.test.package'",
        ''):
      self.device.ForceStop('this.is.a.test.package')


class DeviceUtilsClearApplicationStateTest(DeviceUtilsOldImplTest):

  def testClearApplicationState_packageExists(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'pm path this.package.does.not.exist'",
        ''):
      self.device.ClearApplicationState('this.package.does.not.exist')

  def testClearApplicationState_packageDoesntExist(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'pm path this.package.exists'",
         'package:/data/app/this.package.exists.apk'),
        ("adb -s 0123456789abcdef shell 'pm clear this.package.exists'",
         'Success\r\n')]):
      self.device.ClearApplicationState('this.package.exists')


class DeviceUtilsSendKeyEventTest(DeviceUtilsOldImplTest):

  def testSendKeyEvent(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'input keyevent 66'",
        ''):
      self.device.SendKeyEvent(66)


class DeviceUtilsPushChangedFilesIndividuallyTest(DeviceUtilsNewImplTest):

  def testPushChangedFilesIndividually_empty(self):
    test_files = []
    self.device._PushChangedFilesIndividually(test_files)
    self.assertEqual(0, self.adb.Push.call_count)

  def testPushChangedFilesIndividually_single(self):
    test_files = [('/test/host/path', '/test/device/path')]
    self.device._PushChangedFilesIndividually(test_files)
    self.adb.Push.assert_called_once_with(
        '/test/host/path', '/test/device/path')

  def testPushChangedFilesIndividually_multiple(self):
    test_files = [
        ('/test/host/path/file1', '/test/device/path/file1'),
        ('/test/host/path/file2', '/test/device/path/file2')]
    self.device._PushChangedFilesIndividually(test_files)
    self.assertEqual(2, self.adb.Push.call_count)
    self.adb.Push.assert_any_call(
        '/test/host/path/file1', '/test/device/path/file1')
    self.adb.Push.assert_any_call(
        '/test/host/path/file2', '/test/device/path/file2')


@mock.patch('pylib.device.commands.install_commands.Installed', new=None)
@mock.patch('pylib.device.commands.install_commands.InstallCommands', new=None)
class DeviceUtilsPushChangedFilesZippedTest(DeviceUtilsHybridImplTest):

  def setUp(self):
    super(DeviceUtilsPushChangedFilesZippedTest, self).setUp()

  def testPushChangedFilesZipped_empty(self):
    test_files = []
    self.device._PushChangedFilesZipped(test_files)
    self.assertEqual(0, self.adb.Push.call_count)

  def testPushChangedFilesZipped_single(self):
    test_files = [('/test/host/path/file1', '/test/device/path/file1')]

    self.device._GetExternalStoragePathImpl = mock.Mock(
        return_value='/test/device/external_dir')
    self.device.IsOnline = mock.Mock(return_value=True)
    self.device.RunShellCommand = mock.Mock()
    mock_zip_temp = mock.mock_open()
    mock_zip_temp.return_value.name = '/test/temp/file/tmp.zip'
    with mock.patch('multiprocessing.Process') as mock_zip_proc, (
         mock.patch('tempfile.NamedTemporaryFile', mock_zip_temp)):
      self.device._PushChangedFilesZipped(test_files)

    mock_zip_proc.assert_called_once_with(
        target=device_utils.DeviceUtils._CreateDeviceZip,
        args=('/test/temp/file/tmp.zip', test_files))
    self.adb.Push.assert_called_once_with(
        '/test/temp/file/tmp.zip', '/test/device/external_dir/tmp.zip')
    self.assertEqual(2, self.device.RunShellCommand.call_count)
    self.device.RunShellCommand.assert_any_call(
        ['unzip', '/test/device/external_dir/tmp.zip'],
        as_root=True,
        env={'PATH': '$PATH:/data/local/tmp/bin'},
        check_return=True)
    self.device.RunShellCommand.assert_any_call(
        ['rm', '/test/device/external_dir/tmp.zip'], check_return=True)

  def testPushChangedFilesZipped_multiple(self):
    test_files = [('/test/host/path/file1', '/test/device/path/file1'),
                  ('/test/host/path/file2', '/test/device/path/file2')]

    self.device._GetExternalStoragePathImpl = mock.Mock(
        return_value='/test/device/external_dir')
    self.device.IsOnline = mock.Mock(return_value=True)
    self.device.RunShellCommand = mock.Mock()
    mock_zip_temp = mock.mock_open()
    mock_zip_temp.return_value.name = '/test/temp/file/tmp.zip'
    with mock.patch('multiprocessing.Process') as mock_zip_proc, (
         mock.patch('tempfile.NamedTemporaryFile', mock_zip_temp)):
      self.device._PushChangedFilesZipped(test_files)

    mock_zip_proc.assert_called_once_with(
        target=device_utils.DeviceUtils._CreateDeviceZip,
        args=('/test/temp/file/tmp.zip', test_files))
    self.adb.Push.assert_called_once_with(
        '/test/temp/file/tmp.zip', '/test/device/external_dir/tmp.zip')
    self.assertEqual(2, self.device.RunShellCommand.call_count)
    self.device.RunShellCommand.assert_any_call(
        ['unzip', '/test/device/external_dir/tmp.zip'],
        as_root=True,
        env={'PATH': '$PATH:/data/local/tmp/bin'},
        check_return=True)
    self.device.RunShellCommand.assert_any_call(
        ['rm', '/test/device/external_dir/tmp.zip'], check_return=True)


class DeviceUtilsFileExistsTest(DeviceUtilsOldImplTest):

  def testFileExists_usingTest_fileExists(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell "
            "'test -e \"/data/app/test.file.exists\"; echo $?'",
        '0\r\n'):
      self.assertTrue(self.device.FileExists('/data/app/test.file.exists'))

  def testFileExists_usingTest_fileDoesntExist(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell "
            "'test -e \"/data/app/test.file.does.not.exist\"; echo $?'",
        '1\r\n'):
      self.assertFalse(self.device.FileExists(
          '/data/app/test.file.does.not.exist'))

  def testFileExists_usingLs_fileExists(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell "
            "'test -e \"/data/app/test.file.exists\"; echo $?'",
         'test: not found\r\n'),
        ("adb -s 0123456789abcdef shell "
            "'ls \"/data/app/test.file.exists\" >/dev/null 2>&1; echo $?'",
         '0\r\n')]):
      self.assertTrue(self.device.FileExists('/data/app/test.file.exists'))

  def testFileExists_usingLs_fileDoesntExist(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell "
            "'test -e \"/data/app/test.file.does.not.exist\"; echo $?'",
         'test: not found\r\n'),
        ("adb -s 0123456789abcdef shell "
            "'ls \"/data/app/test.file.does.not.exist\" "
            ">/dev/null 2>&1; echo $?'",
         '1\r\n')]):
      self.assertFalse(self.device.FileExists(
          '/data/app/test.file.does.not.exist'))


class DeviceUtilsPullFileTest(DeviceUtilsOldImplTest):

  def testPullFile_existsOnDevice(self):
    with mock.patch('os.path.exists', return_value=True):
      with self.assertCallsSequence([
          ('adb -s 0123456789abcdef shell '
              'ls /data/app/test.file.exists',
           '/data/app/test.file.exists'),
          ('adb -s 0123456789abcdef pull '
              '/data/app/test.file.exists /test/file/host/path',
           '100 B/s (100 bytes in 1.000s)\r\n')]):
        self.device.PullFile('/data/app/test.file.exists',
                             '/test/file/host/path')

  def testPullFile_doesntExistOnDevice(self):
    with mock.patch('os.path.exists', return_value=True):
      with self.assertCalls(
          'adb -s 0123456789abcdef shell '
              'ls /data/app/test.file.does.not.exist',
          '/data/app/test.file.does.not.exist: No such file or directory\r\n'):
        with self.assertRaises(device_errors.CommandFailedError):
          self.device.PullFile('/data/app/test.file.does.not.exist',
                               '/test/file/host/path')


class DeviceUtilsReadFileTest(DeviceUtilsOldImplTest):

  def testReadFile_exists(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell "
            "'cat \"/read/this/test/file\" 2>/dev/null'",
         'this is a test file')]):
      self.assertEqual(['this is a test file'],
                       self.device.ReadFile('/read/this/test/file'))

  def testReadFile_doesNotExist(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell "
            "'cat \"/this/file/does.not.exist\" 2>/dev/null'",
         ''):
      self.device.ReadFile('/this/file/does.not.exist')

  def testReadFile_asRoot_withRoot(self):
    self.device.old_interface._privileged_command_runner = (
        self.device.old_interface.RunShellCommand)
    self.device.old_interface._protected_file_access_method_initialized = True
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell "
            "'cat \"/this/file/must.be.read.by.root\" 2> /dev/null'",
         'this is a test file\nread by root')]):
      self.assertEqual(
          ['this is a test file', 'read by root'],
          self.device.ReadFile('/this/file/must.be.read.by.root',
                               as_root=True))

  def testReadFile_asRoot_withSu(self):
    self.device.old_interface._privileged_command_runner = (
        self.device.old_interface.RunShellCommandWithSU)
    self.device.old_interface._protected_file_access_method_initialized = True
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell "
            "'su -c cat \"/this/file/can.be.read.with.su\" 2> /dev/null'",
         'this is a test file\nread with su')]):
      self.assertEqual(
          ['this is a test file', 'read with su'],
          self.device.ReadFile('/this/file/can.be.read.with.su',
                               as_root=True))

  def testReadFile_asRoot_rejected(self):
    self.device.old_interface._privileged_command_runner = None
    self.device.old_interface._protected_file_access_method_initialized = True
    with self.assertRaises(device_errors.CommandFailedError):
      self.device.ReadFile('/this/file/cannot.be.read.by.user',
                           as_root=True)


class DeviceUtilsWriteFileTest(DeviceUtilsOldImplTest):

  def testWriteFile_basic(self):
    mock_file = mock.MagicMock(spec=file)
    mock_file.name = '/tmp/file/to.be.pushed'
    mock_file.__enter__.return_value = mock_file
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file):
      with self.assertCalls(
          'adb -s 0123456789abcdef push '
              '/tmp/file/to.be.pushed /test/file/written.to.device',
          '100 B/s (100 bytes in 1.000s)\r\n'):
        self.device.WriteFile('/test/file/written.to.device',
                              'new test file contents')
    mock_file.write.assert_called_once_with('new test file contents')

  def testWriteFile_asRoot_withRoot(self):
    self.device.old_interface._external_storage = '/fake/storage/path'
    self.device.old_interface._privileged_command_runner = (
        self.device.old_interface.RunShellCommand)
    self.device.old_interface._protected_file_access_method_initialized = True

    mock_file = mock.MagicMock(spec=file)
    mock_file.name = '/tmp/file/to.be.pushed'
    mock_file.__enter__.return_value = mock_file
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file):
      with self.assertCallsSequence(
          cmd_ret=[
              # Create temporary contents file
              (r"adb -s 0123456789abcdef shell "
                  "'test -e \"/fake/storage/path/temp_file-\d+-\d+\"; "
                  "echo \$\?'",
               '1\r\n'),
              # Create temporary script file
              (r"adb -s 0123456789abcdef shell "
                  "'test -e \"/fake/storage/path/temp_file-\d+-\d+\.sh\"; "
                  "echo \$\?'",
               '1\r\n'),
              # Set contents file
              (r'adb -s 0123456789abcdef push /tmp/file/to\.be\.pushed '
                  '/fake/storage/path/temp_file-\d+\d+',
               '100 B/s (100 bytes in 1.000s)\r\n'),
              # Set script file
              (r'adb -s 0123456789abcdef push /tmp/file/to\.be\.pushed '
                  '/fake/storage/path/temp_file-\d+\d+',
               '100 B/s (100 bytes in 1.000s)\r\n'),
              # Call script
              (r"adb -s 0123456789abcdef shell "
                  "'sh /fake/storage/path/temp_file-\d+-\d+\.sh'", ''),
              # Remove device temporaries
              (r"adb -s 0123456789abcdef shell "
                  "'rm /fake/storage/path/temp_file-\d+-\d+\.sh'", ''),
              (r"adb -s 0123456789abcdef shell "
                  "'rm /fake/storage/path/temp_file-\d+-\d+'", '')],
          comp=re.match):
        self.device.WriteFile('/test/file/written.to.device',
                              'new test file contents', as_root=True)

  def testWriteFile_asRoot_withSu(self):
    self.device.old_interface._external_storage = '/fake/storage/path'
    self.device.old_interface._privileged_command_runner = (
        self.device.old_interface.RunShellCommandWithSU)
    self.device.old_interface._protected_file_access_method_initialized = True

    mock_file = mock.MagicMock(spec=file)
    mock_file.name = '/tmp/file/to.be.pushed'
    mock_file.__enter__.return_value = mock_file
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file):
      with self.assertCallsSequence(
          cmd_ret=[
              # Create temporary contents file
              (r"adb -s 0123456789abcdef shell "
                  "'test -e \"/fake/storage/path/temp_file-\d+-\d+\"; "
                  "echo \$\?'",
               '1\r\n'),
              # Create temporary script file
              (r"adb -s 0123456789abcdef shell "
                  "'test -e \"/fake/storage/path/temp_file-\d+-\d+\.sh\"; "
                  "echo \$\?'",
               '1\r\n'),
              # Set contents file
              (r'adb -s 0123456789abcdef push /tmp/file/to\.be\.pushed '
                  '/fake/storage/path/temp_file-\d+\d+',
               '100 B/s (100 bytes in 1.000s)\r\n'),
              # Set script file
              (r'adb -s 0123456789abcdef push /tmp/file/to\.be\.pushed '
                  '/fake/storage/path/temp_file-\d+\d+',
               '100 B/s (100 bytes in 1.000s)\r\n'),
              # Call script
              (r"adb -s 0123456789abcdef shell "
                  "'su -c sh /fake/storage/path/temp_file-\d+-\d+\.sh'", ''),
              # Remove device temporaries
              (r"adb -s 0123456789abcdef shell "
                  "'rm /fake/storage/path/temp_file-\d+-\d+\.sh'", ''),
              (r"adb -s 0123456789abcdef shell "
                  "'rm /fake/storage/path/temp_file-\d+-\d+'", '')],
          comp=re.match):
        self.device.WriteFile('/test/file/written.to.device',
                              'new test file contents', as_root=True)

  def testWriteFile_asRoot_rejected(self):
    self.device.old_interface._privileged_command_runner = None
    self.device.old_interface._protected_file_access_method_initialized = True
    with self.assertRaises(device_errors.CommandFailedError):
      self.device.WriteFile('/test/file/no.permissions.to.write',
                            'new test file contents', as_root=True)

class DeviceUtilsWriteTextFileTest(DeviceUtilsNewImplTest):

  def testWriteTextFileTest_basic(self):
    with self.assertShellCall('echo some.string > /test/file/to.write'):
      self.device.WriteTextFile('/test/file/to.write', 'some.string')

  def testWriteTextFileTest_quoted(self):
    with self.assertShellCall(
        "echo 'some other string' > '/test/file/to write'"):
      self.device.WriteTextFile('/test/file/to write', 'some other string')

  def testWriteTextFileTest_asRoot(self):
    with self.assertShellCallSequence([
        ('su -c ls /root && ! ls /root', ''),
        ('su -c echo string > /test/file', '')]):
      self.device.WriteTextFile('/test/file', 'string', as_root=True)

class DeviceUtilsLsTest(DeviceUtilsOldImplTest):

  def testLs_nothing(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls -lR /this/file/does.not.exist'",
         '/this/file/does.not.exist: No such file or directory\r\n'),
        ("adb -s 0123456789abcdef shell 'date +%z'", '+0000')]):
      self.assertEqual({}, self.device.Ls('/this/file/does.not.exist'))

  def testLs_file(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls -lR /this/is/a/test.file'",
         '-rw-rw---- testuser testgroup 4096 1970-01-01 00:00 test.file\r\n'),
        ("adb -s 0123456789abcdef shell 'date +%z'", '+0000')]):
      self.assertEqual(
          {'test.file': (4096, datetime.datetime(1970, 1, 1))},
          self.device.Ls('/this/is/a/test.file'))

  def testLs_directory(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls -lR /this/is/a/test.directory'",
         '\r\n'
         '/this/is/a/test.directory:\r\n'
         '-rw-rw---- testuser testgroup 4096 1970-01-01 18:19 test.file\r\n'),
        ("adb -s 0123456789abcdef shell 'date +%z'", '+0000')]):
      self.assertEqual(
          {'test.file': (4096, datetime.datetime(1970, 1, 1, 18, 19))},
          self.device.Ls('/this/is/a/test.directory'))

  def testLs_directories(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'ls -lR /this/is/a/test.directory'",
         '\r\n'
         '/this/is/a/test.directory:\r\n'
         'drwxr-xr-x testuser testgroup 1970-01-01 00:00 test.subdirectory\r\n'
         '\r\n'
         '/this/is/a/test.directory/test.subdirectory:\r\n'
         '-rw-rw---- testuser testgroup 4096 1970-01-01 00:00 test.file\r\n'),
        ("adb -s 0123456789abcdef shell 'date +%z'", '-0700')]):
      self.assertEqual(
          {'test.subdirectory/test.file':
              (4096, datetime.datetime(1970, 1, 1, 7, 0, 0))},
          self.device.Ls('/this/is/a/test.directory'))


class DeviceUtilsSetJavaAssertsTest(DeviceUtilsOldImplTest):

  @staticmethod
  def mockNamedTemporary(name='/tmp/file/property.file',
                         read_contents=''):
    mock_file = mock.MagicMock(spec=file)
    mock_file.name = name
    mock_file.__enter__.return_value = mock_file
    mock_file.read.return_value = read_contents
    return mock_file

  def testSetJavaAsserts_enable(self):
    mock_file = self.mockNamedTemporary()
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file), (
         mock.patch('__builtin__.open', return_value=mock_file)):
      with self.assertCallsSequence(
          [('adb -s 0123456789abcdef shell ls %s' %
                constants.DEVICE_LOCAL_PROPERTIES_PATH,
            '%s\r\n' % constants.DEVICE_LOCAL_PROPERTIES_PATH),
           ('adb -s 0123456789abcdef pull %s %s' %
                (constants.DEVICE_LOCAL_PROPERTIES_PATH, mock_file.name),
            '100 B/s (100 bytes in 1.000s)\r\n'),
           ('adb -s 0123456789abcdef push %s %s' %
                (mock_file.name, constants.DEVICE_LOCAL_PROPERTIES_PATH),
            '100 B/s (100 bytes in 1.000s)\r\n'),
           ('adb -s 0123456789abcdef shell '
                'getprop dalvik.vm.enableassertions',
            '\r\n'),
           ('adb -s 0123456789abcdef shell '
                'setprop dalvik.vm.enableassertions "all"',
            '')]):
        self.assertTrue(self.device.SetJavaAsserts(True))

  def testSetJavaAsserts_disable(self):
    mock_file = self.mockNamedTemporary(
        read_contents='dalvik.vm.enableassertions=all\n')
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file), (
         mock.patch('__builtin__.open', return_value=mock_file)):
      with self.assertCallsSequence(
          [('adb -s 0123456789abcdef shell ls %s' %
                constants.DEVICE_LOCAL_PROPERTIES_PATH,
            '%s\r\n' % constants.DEVICE_LOCAL_PROPERTIES_PATH),
           ('adb -s 0123456789abcdef pull %s %s' %
                (constants.DEVICE_LOCAL_PROPERTIES_PATH, mock_file.name),
            '100 B/s (100 bytes in 1.000s)\r\n'),
           ('adb -s 0123456789abcdef push %s %s' %
                (mock_file.name, constants.DEVICE_LOCAL_PROPERTIES_PATH),
            '100 B/s (100 bytes in 1.000s)\r\n'),
           ('adb -s 0123456789abcdef shell '
                'getprop dalvik.vm.enableassertions',
            'all\r\n'),
           ('adb -s 0123456789abcdef shell '
                'setprop dalvik.vm.enableassertions ""',
            '')]):
        self.assertTrue(self.device.SetJavaAsserts(False))

  def testSetJavaAsserts_alreadyEnabled(self):
    mock_file = self.mockNamedTemporary(
        read_contents='dalvik.vm.enableassertions=all\n')
    with mock.patch('tempfile.NamedTemporaryFile',
                    return_value=mock_file), (
         mock.patch('__builtin__.open', return_value=mock_file)):
      with self.assertCallsSequence(
          [('adb -s 0123456789abcdef shell ls %s' %
                constants.DEVICE_LOCAL_PROPERTIES_PATH,
            '%s\r\n' % constants.DEVICE_LOCAL_PROPERTIES_PATH),
           ('adb -s 0123456789abcdef pull %s %s' %
                (constants.DEVICE_LOCAL_PROPERTIES_PATH, mock_file.name),
            '100 B/s (100 bytes in 1.000s)\r\n'),
           ('adb -s 0123456789abcdef shell '
                'getprop dalvik.vm.enableassertions',
            'all\r\n')]):
        self.assertFalse(self.device.SetJavaAsserts(True))


class DeviceUtilsGetPropTest(DeviceUtilsNewImplTest):

  def testGetProp_exists(self):
    with self.assertShellCall('getprop this.is.a.test.property',
                              'test_property_value\r\n'):
      self.assertEqual('test_property_value',
                       self.device.GetProp('this.is.a.test.property'))

  def testGetProp_doesNotExist(self):
    with self.assertShellCall('getprop this.property.does.not.exist',
                              '\r\n'):
      self.assertEqual('', self.device.GetProp('this.property.does.not.exist'))

  def testGetProp_cachedRoProp(self):
    with self.assertShellCall('getprop ro.build.type',
                              'userdebug\r\n'):
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type', cache=True))
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type', cache=True))

  def testGetProp_retryAndCache(self):
    with self.assertShellCallSequence([
        ('getprop ro.build.type', _ShellError()),
        ('getprop ro.build.type', _ShellError()),
        ('getprop ro.build.type', 'userdebug\r\n')]):
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type',
                                           cache=True, retries=3))
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type',
                                           cache=True, retries=3))


class DeviceUtilsSetPropTest(DeviceUtilsNewImplTest):

  def testSetProp(self):
    with self.assertShellCall(
      "setprop this.is.a.test.property 'test property value'"):
      self.device.SetProp('this.is.a.test.property', 'test property value')


class DeviceUtilsGetPidsTest(DeviceUtilsNewImplTest):

  def testGetPids_noMatches(self):
    with self.assertShellCall(
        'ps',
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
        'user  1000    100   1024 1024   ffffffff 00000000 no.match\r\n'):
      self.assertEqual({}, self.device.GetPids('does.not.match'))

  def testGetPids_oneMatch(self):
    with self.assertShellCall(
        'ps',
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not.a.match\r\n'
        'user  1001    100   1024 1024   ffffffff 00000000 one.match\r\n'):
      self.assertEqual({'one.match': '1001'}, self.device.GetPids('one.match'))

  def testGetPids_mutlipleMatches(self):
    with self.assertShellCall(
        'ps',
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not\r\n'
        'user  1001    100   1024 1024   ffffffff 00000000 one.match\r\n'
        'user  1002    100   1024 1024   ffffffff 00000000 two.match\r\n'
        'user  1003    100   1024 1024   ffffffff 00000000 three.match\r\n'):
      self.assertEqual(
          {'one.match': '1001', 'two.match': '1002', 'three.match': '1003'},
          self.device.GetPids('match'))

  def testGetPids_exactMatch(self):
    with self.assertShellCall(
        'ps',
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\r\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not.exact.match\r\n'
        'user  1234    100   1024 1024   ffffffff 00000000 exact.match\r\n'):
      self.assertEqual(
          {'not.exact.match': '1000', 'exact.match': '1234'},
          self.device.GetPids('exact.match'))


class DeviceUtilsTakeScreenshotTest(DeviceUtilsOldImplTest):

  def testTakeScreenshot_fileNameProvided(self):
    mock_fs = MockFileSystem()
    mock_fs.addMockDirectory('/test/host')
    mock_fs.addMockFile('/test/host/screenshot.png')

    with mock_fs:
      with self.assertCallsSequence(
          cmd_ret=[
              (r"adb -s 0123456789abcdef shell 'echo \$EXTERNAL_STORAGE'",
               '/test/external/storage\r\n'),
              (r"adb -s 0123456789abcdef shell '/system/bin/screencap -p \S+'",
               ''),
              (r"adb -s 0123456789abcdef shell ls \S+",
               '/test/external/storage/screenshot.png\r\n'),
              (r'adb -s 0123456789abcdef pull \S+ /test/host/screenshot.png',
               '100 B/s (100 B in 1.000s)\r\n'),
              (r"adb -s 0123456789abcdef shell 'rm -f \S+'", '')
          ],
          comp=re.match):
        self.device.TakeScreenshot('/test/host/screenshot.png')


class DeviceUtilsGetIOStatsTest(DeviceUtilsOldImplTest):

  def testGetIOStats(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'cat \"/proc/diskstats\" 2>/dev/null'",
        '179 0 mmcblk0 1 2 3 4 5 6 7 8 9 10 11\r\n'):
      self.assertEqual(
          {
            'num_reads': 1,
            'num_writes': 5,
            'read_ms': 4,
            'write_ms': 8,
          },
          self.device.GetIOStats())


class DeviceUtilsGetMemoryUsageForPidTest(DeviceUtilsOldImplTest):

  def setUp(self):
    super(DeviceUtilsGetMemoryUsageForPidTest, self).setUp()
    self.device.old_interface._privileged_command_runner = (
        self.device.old_interface.RunShellCommand)
    self.device.old_interface._protected_file_access_method_initialized = True

  def testGetMemoryUsageForPid_validPid(self):
    with self.assertCallsSequence([
        ("adb -s 0123456789abcdef shell 'showmap 1234'",
         '100 101 102 103 104 105 106 107 TOTAL\r\n'),
        ("adb -s 0123456789abcdef shell "
            "'cat \"/proc/1234/status\" 2> /dev/null'",
         'VmHWM: 1024 kB')
        ]):
      self.assertEqual(
          {
            'Size': 100,
            'Rss': 101,
            'Pss': 102,
            'Shared_Clean': 103,
            'Shared_Dirty': 104,
            'Private_Clean': 105,
            'Private_Dirty': 106,
            'VmHWM': 1024
          },
          self.device.GetMemoryUsageForPid(1234))

  def testGetMemoryUsageForPid_invalidPid(self):
    with self.assertCalls(
        "adb -s 0123456789abcdef shell 'showmap 4321'",
        'cannot open /proc/4321/smaps: No such file or directory\r\n'):
      self.assertEqual({}, self.device.GetMemoryUsageForPid(4321))


class DeviceUtilsStrTest(DeviceUtilsOldImplTest):
  def testStr_noAdbCalls(self):
    with self.assertNoAdbCalls():
      self.assertEqual('0123456789abcdef', str(self.device))

  def testStr_noSerial(self):
    self.device = device_utils.DeviceUtils(None)
    with self.assertCalls('adb  get-serialno', '0123456789abcdef'):
      self.assertEqual('0123456789abcdef', str(self.device))

  def testStr_noSerial_noDevices(self):
    self.device = device_utils.DeviceUtils(None)
    with self.assertCalls('adb  get-serialno', 'unknown'), (
         self.assertRaises(device_errors.NoDevicesError)):
      str(self.device)


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)

