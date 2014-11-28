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
from pylib.utils import mock_calls

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


class MockTempFile(object):

  def __init__(self, name='/tmp/some/file'):
    self.file = mock.MagicMock(spec=file)
    self.file.name = name

  def __enter__(self):
    return self.file

  def __exit__(self, exc_type, exc_val, exc_tb):
    pass


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
    self._get_adb_path_patch = mock.patch('pylib.constants.GetAdbPath',
                                          mock.Mock(return_value='adb'))
    self._get_adb_path_patch.start()
    self.device = device_utils.DeviceUtils(
        '0123456789abcdef', default_timeout=1, default_retries=0)

  def tearDown(self):
    self._get_adb_path_patch.stop()

class DeviceUtilsNewImplTest(mock_calls.TestCase):

  def setUp(self):
    test_serial = '0123456789abcdef'
    self.adb = mock.Mock(spec=adb_wrapper.AdbWrapper)
    self.adb.__str__ = mock.Mock(return_value=test_serial)
    self.adb.GetDeviceSerial.return_value = test_serial
    self.device = device_utils.DeviceUtils(
        self.adb, default_timeout=10, default_retries=0)
    self.watchMethodCalls(self.call.adb)

  def ShellError(self, output=None, exit_code=1):
    def action(cmd, *args, **kwargs):
      raise device_errors.AdbCommandFailedError(
          cmd, output, exit_code, str(self.device))
    if output is None:
      output = 'Permission denied\n'
    return action

  def TimeoutError(self, msg=None):
    if msg is None:
      msg = 'Operation timed out'
    return mock.Mock(side_effect=device_errors.CommandTimeoutError(
        msg, str(self.device)))

  def CommandError(self, msg=None):
    if msg is None:
      msg = 'Command failed'
    return mock.Mock(side_effect=device_errors.CommandFailedError(
        msg, str(self.device)))


class DeviceUtilsIsOnlineTest(DeviceUtilsNewImplTest):

  def testIsOnline_true(self):
    with self.assertCall(self.call.adb.GetState(), 'device'):
      self.assertTrue(self.device.IsOnline())

  def testIsOnline_false(self):
    with self.assertCall(self.call.adb.GetState(), 'offline'):
      self.assertFalse(self.device.IsOnline())

  def testIsOnline_error(self):
    with self.assertCall(self.call.adb.GetState(), self.CommandError()):
      self.assertFalse(self.device.IsOnline())


class DeviceUtilsHasRootTest(DeviceUtilsNewImplTest):

  def testHasRoot_true(self):
    with self.assertCall(self.call.adb.Shell('ls /root'), 'foo\n'):
      self.assertTrue(self.device.HasRoot())

  def testHasRoot_false(self):
    with self.assertCall(self.call.adb.Shell('ls /root'), self.ShellError()):
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
    with self.assertCall(
        self.call.device.GetProp('ro.build.type', cache=True), 'user'):
      self.assertTrue(self.device.IsUserBuild())

  def testIsUserBuild_no(self):
    with self.assertCall(
        self.call.device.GetProp('ro.build.type', cache=True), 'userdebug'):
      self.assertFalse(self.device.IsUserBuild())


class DeviceUtilsGetExternalStoragePathTest(DeviceUtilsNewImplTest):

  def testGetExternalStoragePath_succeeds(self):
    with self.assertCall(
        self.call.adb.Shell('echo $EXTERNAL_STORAGE'), '/fake/storage/path\n'):
      self.assertEquals('/fake/storage/path',
                        self.device.GetExternalStoragePath())

  def testGetExternalStoragePath_fails(self):
    with self.assertCall(self.call.adb.Shell('echo $EXTERNAL_STORAGE'), '\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetExternalStoragePath()


class DeviceUtilsGetApplicationPathTest(DeviceUtilsNewImplTest):

  def testGetApplicationPath_exists(self):
    with self.assertCall(self.call.adb.Shell('pm path android'),
                         'package:/path/to/android.apk\n'):
      self.assertEquals('/path/to/android.apk',
                        self.device.GetApplicationPath('android'))

  def testGetApplicationPath_notExists(self):
    with self.assertCall(self.call.adb.Shell('pm path not.installed.app'), ''):
      self.assertEquals(None,
                        self.device.GetApplicationPath('not.installed.app'))

  def testGetApplicationPath_fails(self):
    with self.assertCall(self.call.adb.Shell('pm path android'),
        self.CommandError('ERROR. Is package manager running?\n')):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetApplicationPath('android')


@mock.patch('time.sleep', mock.Mock())
class DeviceUtilsWaitUntilFullyBootedTest(DeviceUtilsNewImplTest):

  def testWaitUntilFullyBooted_succeedsNoWifi(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), ''),
        # pm_ready
        (self.call.device.GetApplicationPath('android'),
         'package:/some/fake/path'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), '1')):
      self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_succeedsWithWifi(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), ''),
        # pm_ready
        (self.call.device.GetApplicationPath('android'),
         'package:/some/fake/path'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), '1'),
        # wifi_enabled
        (self.call.adb.Shell('dumpsys wifi'),
         'stuff\nWi-Fi is enabled\nmore stuff\n')):
      self.device.WaitUntilFullyBooted(wifi=True)

  def testWaitUntilFullyBooted_sdCardReadyFails_noPath(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), self.CommandError())):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_sdCardReadyFails_notExists(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), self.ShellError()),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), self.ShellError()),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'),
         self.TimeoutError())):
      with self.assertRaises(device_errors.CommandTimeoutError):
        self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_devicePmFails(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), ''),
        # pm_ready
        (self.call.device.GetApplicationPath('android'), self.CommandError()),
        # pm_ready
        (self.call.device.GetApplicationPath('android'), self.CommandError()),
        # pm_ready
        (self.call.device.GetApplicationPath('android'), self.TimeoutError())):
      with self.assertRaises(device_errors.CommandTimeoutError):
        self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_bootFails(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), ''),
        # pm_ready
        (self.call.device.GetApplicationPath('android'),
         'package:/some/fake/path'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), '0'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), '0'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), self.TimeoutError())):
      with self.assertRaises(device_errors.CommandTimeoutError):
        self.device.WaitUntilFullyBooted(wifi=False)

  def testWaitUntilFullyBooted_wifiFails(self):
    with self.assertCalls(
        self.call.adb.WaitForDevice(),
        # sd_card_ready
        (self.call.device.GetExternalStoragePath(), '/fake/storage/path'),
        (self.call.adb.Shell('test -d /fake/storage/path'), ''),
        # pm_ready
        (self.call.device.GetApplicationPath('android'),
         'package:/some/fake/path'),
        # boot_completed
        (self.call.device.GetProp('sys.boot_completed'), '1'),
        # wifi_enabled
        (self.call.adb.Shell('dumpsys wifi'), 'stuff\nmore stuff\n'),
        # wifi_enabled
        (self.call.adb.Shell('dumpsys wifi'), 'stuff\nmore stuff\n'),
        # wifi_enabled
        (self.call.adb.Shell('dumpsys wifi'), self.TimeoutError())):
      with self.assertRaises(device_errors.CommandTimeoutError):
        self.device.WaitUntilFullyBooted(wifi=True)


@mock.patch('time.sleep', mock.Mock())
class DeviceUtilsRebootTest(DeviceUtilsNewImplTest):

  def testReboot_nonBlocking(self):
    with self.assertCalls(
        self.call.adb.Reboot(),
        (self.call.device.IsOnline(), True),
        (self.call.device.IsOnline(), False)):
      self.device.Reboot(block=False)

  def testReboot_blocking(self):
    with self.assertCalls(
        self.call.adb.Reboot(),
        (self.call.device.IsOnline(), True),
        (self.call.device.IsOnline(), False),
        self.call.device.WaitUntilFullyBooted()):
      self.device.Reboot(block=True)


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

    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('os.path.exists', return_value=True)), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')), (
         mock.patch('pylib.constants.GetOutDirectory',
                    return_value='/fake/test/out')), (
         mock.patch('pylib.android_commands.AndroidCommands.GetFilesChanged',
                    side_effect=mockGetFilesChanged)):
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

    with mock.patch('os.path.isfile', return_value=True), (
         mock.patch('pylib.utils.apk_helper.GetPackageName',
                    return_value='this.is.a.test.package')), (
         mock.patch('pylib.constants.GetOutDirectory',
                    return_value='/fake/test/out')), (
         mock.patch('pylib.android_commands.AndroidCommands.GetFilesChanged',
                    side_effect=mockGetFilesChanged)):
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

  def setUp(self):
    super(DeviceUtilsRunShellCommandTest, self).setUp()
    self.device.NeedsSU = mock.Mock(return_value=False)

  def testRunShellCommand_commandAsList(self):
    with self.assertCall(self.call.adb.Shell('pm list packages'), ''):
      self.device.RunShellCommand(['pm', 'list', 'packages'])

  def testRunShellCommand_commandAsListQuoted(self):
    with self.assertCall(self.call.adb.Shell("echo 'hello world' '$10'"), ''):
      self.device.RunShellCommand(['echo', 'hello world', '$10'])

  def testRunShellCommand_commandAsString(self):
    with self.assertCall(self.call.adb.Shell('echo "$VAR"'), ''):
      self.device.RunShellCommand('echo "$VAR"')

  def testNewRunShellImpl_withEnv(self):
    with self.assertCall(
        self.call.adb.Shell('VAR=some_string echo "$VAR"'), ''):
      self.device.RunShellCommand('echo "$VAR"', env={'VAR': 'some_string'})

  def testNewRunShellImpl_withEnvQuoted(self):
    with self.assertCall(
        self.call.adb.Shell('PATH="$PATH:/other/path" run_this'), ''):
      self.device.RunShellCommand('run_this', env={'PATH': '$PATH:/other/path'})

  def testNewRunShellImpl_withEnv_failure(self):
    with self.assertRaises(KeyError):
      self.device.RunShellCommand('some_cmd', env={'INVALID NAME': 'value'})

  def testNewRunShellImpl_withCwd(self):
    with self.assertCall(self.call.adb.Shell('cd /some/test/path && ls'), ''):
      self.device.RunShellCommand('ls', cwd='/some/test/path')

  def testNewRunShellImpl_withCwdQuoted(self):
    with self.assertCall(
        self.call.adb.Shell("cd '/some test/path with/spaces' && ls"), ''):
      self.device.RunShellCommand('ls', cwd='/some test/path with/spaces')

  def testRunShellCommand_withSu(self):
    with self.assertCalls(
        (self.call.device.NeedsSU(), True),
        (self.call.adb.Shell("su -c sh -c 'setprop service.adb.root 0'"), '')):
      self.device.RunShellCommand('setprop service.adb.root 0', as_root=True)

  def testRunShellCommand_manyLines(self):
    cmd = 'ls /some/path'
    with self.assertCall(self.call.adb.Shell(cmd), 'file1\nfile2\nfile3\n'):
      self.assertEquals(['file1', 'file2', 'file3'],
                        self.device.RunShellCommand(cmd))

  def testRunShellCommand_singleLine_success(self):
    cmd = 'echo $VALUE'
    with self.assertCall(self.call.adb.Shell(cmd), 'some value\n'):
      self.assertEquals('some value',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successEmptyLine(self):
    cmd = 'echo $VALUE'
    with self.assertCall(self.call.adb.Shell(cmd), '\n'):
      self.assertEquals('',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successWithoutEndLine(self):
    cmd = 'echo -n $VALUE'
    with self.assertCall(self.call.adb.Shell(cmd), 'some value'):
      self.assertEquals('some value',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_successNoOutput(self):
    cmd = 'echo -n $VALUE'
    with self.assertCall(self.call.adb.Shell(cmd), ''):
      self.assertEquals('',
                        self.device.RunShellCommand(cmd, single_line=True))

  def testRunShellCommand_singleLine_failTooManyLines(self):
    cmd = 'echo $VALUE'
    with self.assertCall(self.call.adb.Shell(cmd),
                         'some value\nanother value\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.RunShellCommand(cmd, single_line=True)

  def testRunShellCommand_checkReturn_success(self):
    cmd = 'echo $ANDROID_DATA'
    output = '/data\n'
    with self.assertCall(self.call.adb.Shell(cmd), output):
      self.assertEquals([output.rstrip()],
                        self.device.RunShellCommand(cmd, check_return=True))

  def testRunShellCommand_checkReturn_failure(self):
    cmd = 'ls /root'
    output = 'opendir failed, Permission denied\n'
    with self.assertCall(self.call.adb.Shell(cmd), self.ShellError(output)):
      with self.assertRaises(device_errors.AdbCommandFailedError):
        self.device.RunShellCommand(cmd, check_return=True)

  def testRunShellCommand_checkReturn_disabled(self):
    cmd = 'ls /root'
    output = 'opendir failed, Permission denied\n'
    with self.assertCall(self.call.adb.Shell(cmd), self.ShellError(output)):
      self.assertEquals([output.rstrip()],
                        self.device.RunShellCommand(cmd, check_return=False))


@mock.patch('time.sleep', mock.Mock())
class DeviceUtilsKillAllTest(DeviceUtilsNewImplTest):

  def testKillAll_noMatchingProcesses(self):
    with self.assertCall(self.call.adb.Shell('ps'),
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.KillAll('test_process')

  def testKillAll_nonblocking(self):
    with self.assertCalls(
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
         'u0_a1  1234  174   123456 54321 ffffffff 456789ab some.process\n'),
        (self.call.adb.Shell('kill -9 1234'), '')):
      self.assertEquals(1,
          self.device.KillAll('some.process', blocking=False))

  def testKillAll_blocking(self):
    with self.assertCalls(
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
         'u0_a1  1234  174   123456 54321 ffffffff 456789ab some.process\n'),
        (self.call.adb.Shell('kill -9 1234'), ''),
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
         'u0_a1  1234  174   123456 54321 ffffffff 456789ab some.process\n'),
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n')):
      self.assertEquals(1,
          self.device.KillAll('some.process', blocking=True))

  def testKillAll_root(self):
    with self.assertCalls(
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
         'u0_a1  1234  174   123456 54321 ffffffff 456789ab some.process\n'),
        (self.call.device.NeedsSU(), True),
        (self.call.adb.Shell("su -c sh -c 'kill -9 1234'"), '')):
      self.assertEquals(1,
          self.device.KillAll('some.process', as_root=True))

  def testKillAll_sigterm(self):
    with self.assertCalls(
        (self.call.adb.Shell('ps'),
         'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
         'u0_a1  1234  174   123456 54321 ffffffff 456789ab some.process\n'),
        (self.call.adb.Shell('kill -15 1234'), '')):
      self.assertEquals(1,
          self.device.KillAll('some.process', signum=signal.SIGTERM))


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


class DeviceUtilsStartInstrumentationTest(DeviceUtilsNewImplTest):

  def testStartInstrumentation_nothing(self):
    with self.assertCalls(
        self.call.device.RunShellCommand(
            ['am', 'instrument', 'test.package/.TestInstrumentation'],
            check_return=True)):
      self.device.StartInstrumentation(
          'test.package/.TestInstrumentation',
          finish=False, raw=False, extras=None)

  def testStartInstrumentation_finish(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['am', 'instrument', '-w', 'test.package/.TestInstrumentation'],
            check_return=True),
         ['OK (1 test)'])):
      output = self.device.StartInstrumentation(
          'test.package/.TestInstrumentation',
          finish=True, raw=False, extras=None)
      self.assertEquals(['OK (1 test)'], output)

  def testStartInstrumentation_raw(self):
    with self.assertCalls(
        self.call.device.RunShellCommand(
            ['am', 'instrument', '-r', 'test.package/.TestInstrumentation'],
            check_return=True)):
      self.device.StartInstrumentation(
          'test.package/.TestInstrumentation',
          finish=False, raw=True, extras=None)

  def testStartInstrumentation_extras(self):
    with self.assertCalls(
        self.call.device.RunShellCommand(
            ['am', 'instrument', '-e', 'foo', 'Foo', '-e', 'bar', 'Bar',
             'test.package/.TestInstrumentation'],
            check_return=True)):
      self.device.StartInstrumentation(
          'test.package/.TestInstrumentation',
          finish=False, raw=False, extras={'foo': 'Foo', 'bar': 'Bar'})


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
    with self.assertCalls():
      self.device._PushChangedFilesIndividually(test_files)

  def testPushChangedFilesIndividually_single(self):
    test_files = [('/test/host/path', '/test/device/path')]
    with self.assertCalls(self.call.adb.Push(*test_files[0])):
      self.device._PushChangedFilesIndividually(test_files)

  def testPushChangedFilesIndividually_multiple(self):
    test_files = [
        ('/test/host/path/file1', '/test/device/path/file1'),
        ('/test/host/path/file2', '/test/device/path/file2')]
    with self.assertCalls(
        self.call.adb.Push(*test_files[0]),
        self.call.adb.Push(*test_files[1])):
      self.device._PushChangedFilesIndividually(test_files)


class DeviceUtilsPushChangedFilesZippedTest(DeviceUtilsNewImplTest):

  def testPushChangedFilesZipped_empty(self):
    test_files = []
    with self.assertCalls():
      self.device._PushChangedFilesZipped(test_files)

  def _testPushChangedFilesZipped_spec(self, test_files):
    mock_zip_temp = mock.mock_open()
    mock_zip_temp.return_value.name = '/test/temp/file/tmp.zip'
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(suffix='.zip'), mock_zip_temp),
        (mock.call.multiprocessing.Process(
            target=device_utils.DeviceUtils._CreateDeviceZip,
            args=('/test/temp/file/tmp.zip', test_files)), mock.Mock()),
        (self.call.device.GetExternalStoragePath(),
         '/test/device/external_dir'),
        self.call.adb.Push(
            '/test/temp/file/tmp.zip', '/test/device/external_dir/tmp.zip'),
        self.call.device.RunShellCommand(
            ['unzip', '/test/device/external_dir/tmp.zip'],
            as_root=True,
            env={'PATH': '$PATH:/data/local/tmp/bin'},
            check_return=True),
        (self.call.device.IsOnline(), True),
        self.call.device.RunShellCommand(
            ['rm', '/test/device/external_dir/tmp.zip'], check_return=True)):
      self.device._PushChangedFilesZipped(test_files)

  def testPushChangedFilesZipped_single(self):
    self._testPushChangedFilesZipped_spec(
        [('/test/host/path/file1', '/test/device/path/file1')])

  def testPushChangedFilesZipped_multiple(self):
    self._testPushChangedFilesZipped_spec(
        [('/test/host/path/file1', '/test/device/path/file1'),
         ('/test/host/path/file2', '/test/device/path/file2')])


class DeviceUtilsFileExistsTest(DeviceUtilsNewImplTest):

  def testFileExists_usingTest_fileExists(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['test', '-e', '/path/file.exists'], check_return=True), ''):
      self.assertTrue(self.device.FileExists('/path/file.exists'))

  def testFileExists_usingTest_fileDoesntExist(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['test', '-e', '/does/not/exist'], check_return=True),
        self.ShellError('', 1)):
      self.assertFalse(self.device.FileExists('/does/not/exist'))


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


class DeviceUtilsWriteFileTest(DeviceUtilsNewImplTest):

  def testWriteFile_withPush(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'some large contents ' * 26 # 20 * 26 = 520 chars
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        self.call.adb.Push('/tmp/file/on.host', '/path/to/device/file')):
      self.device.WriteFile('/path/to/device/file', contents)
      tmp_host.file.write.assert_called_once_with(contents)

  def testWriteFile_withPushForced(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'tiny contents'
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        self.call.adb.Push('/tmp/file/on.host', '/path/to/device/file')):
      self.device.WriteFile('/path/to/device/file', contents, force_push=True)
      tmp_host.file.write.assert_called_once_with(contents)

  def testWriteFile_withPushAndSU(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'some large contents ' * 26 # 20 * 26 = 520 chars
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        (self.call.device.NeedsSU(), True),
        (mock.call.pylib.utils.device_temp_file.DeviceTempFile(self.device),
         MockTempFile('/external/path/tmp/on.device')),
        self.call.adb.Push('/tmp/file/on.host', '/external/path/tmp/on.device'),
        self.call.device.RunShellCommand(
            ['cp', '/external/path/tmp/on.device', '/path/to/device/file'],
            as_root=True, check_return=True)):
      self.device.WriteFile('/path/to/device/file', contents, as_root=True)
      tmp_host.file.write.assert_called_once_with(contents)

  def testWriteFile_withPush_rejected(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'some large contents ' * 26 # 20 * 26 = 520 chars
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        (self.call.adb.Push('/tmp/file/on.host', '/path/to/device/file'),
         self.CommandError())):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.WriteFile('/path/to/device/file', contents)

  def testWriteFile_withEcho(self):
    with self.assertCall(self.call.adb.Shell(
        "echo -n the.contents > /test/file/to.write"), ''):
      self.device.WriteFile('/test/file/to.write', 'the.contents')

  def testWriteFile_withEchoAndQuotes(self):
    with self.assertCall(self.call.adb.Shell(
        "echo -n 'the contents' > '/test/file/to write'"), ''):
      self.device.WriteFile('/test/file/to write', 'the contents')

  def testWriteFile_withEchoAndSU(self):
    with self.assertCalls(
        (self.call.device.NeedsSU(), True),
        (self.call.adb.Shell("su -c sh -c 'echo -n contents > /test/file'"),
         '')):
      self.device.WriteFile('/test/file', 'contents', as_root=True)


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
    with self.assertCall(
        self.call.adb.Shell('getprop test.property'), 'property_value\n'):
      self.assertEqual('property_value',
                       self.device.GetProp('test.property'))

  def testGetProp_doesNotExist(self):
    with self.assertCall(
        self.call.adb.Shell('getprop property.does.not.exist'), '\n'):
      self.assertEqual('', self.device.GetProp('property.does.not.exist'))

  def testGetProp_cachedRoProp(self):
    with self.assertCall(
        self.call.adb.Shell('getprop ro.build.type'), 'userdebug\n'):
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type', cache=True))
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type', cache=True))

  def testGetProp_retryAndCache(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.type'), self.ShellError()),
        (self.call.adb.Shell('getprop ro.build.type'), self.ShellError()),
        (self.call.adb.Shell('getprop ro.build.type'), 'userdebug\n')):
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type',
                                           cache=True, retries=3))
      self.assertEqual('userdebug',
                       self.device.GetProp('ro.build.type',
                                           cache=True, retries=3))


class DeviceUtilsSetPropTest(DeviceUtilsNewImplTest):

  def testSetProp(self):
    with self.assertCall(
        self.call.adb.Shell("setprop test.property 'test value'"), ''):
      self.device.SetProp('test.property', 'test value')

  def testSetProp_check_succeeds(self):
    with self.assertCalls(
        (self.call.adb.Shell('setprop test.property new_value'), ''),
        (self.call.adb.Shell('getprop test.property'), 'new_value')):
      self.device.SetProp('test.property', 'new_value', check=True)

  def testSetProp_check_fails(self):
    with self.assertCalls(
        (self.call.adb.Shell('setprop test.property new_value'), ''),
        (self.call.adb.Shell('getprop test.property'), 'old_value')):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.SetProp('test.property', 'new_value', check=True)


class DeviceUtilsGetPidsTest(DeviceUtilsNewImplTest):

  def testGetPids_noMatches(self):
    with self.assertCall(self.call.adb.Shell('ps'),
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
        'user  1000    100   1024 1024   ffffffff 00000000 no.match\n'):
      self.assertEqual({}, self.device.GetPids('does.not.match'))

  def testGetPids_oneMatch(self):
    with self.assertCall(self.call.adb.Shell('ps'),
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not.a.match\n'
        'user  1001    100   1024 1024   ffffffff 00000000 one.match\n'):
      self.assertEqual({'one.match': '1001'}, self.device.GetPids('one.match'))

  def testGetPids_mutlipleMatches(self):
    with self.assertCall(self.call.adb.Shell('ps'),
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not\n'
        'user  1001    100   1024 1024   ffffffff 00000000 one.match\n'
        'user  1002    100   1024 1024   ffffffff 00000000 two.match\n'
        'user  1003    100   1024 1024   ffffffff 00000000 three.match\n'):
      self.assertEqual(
          {'one.match': '1001', 'two.match': '1002', 'three.match': '1003'},
          self.device.GetPids('match'))

  def testGetPids_exactMatch(self):
    with self.assertCall(self.call.adb.Shell('ps'),
        'USER   PID   PPID  VSIZE  RSS   WCHAN    PC       NAME\n'
        'user  1000    100   1024 1024   ffffffff 00000000 not.exact.match\n'
        'user  1234    100   1024 1024   ffffffff 00000000 exact.match\n'):
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

