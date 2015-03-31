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
from pylib import cmd_helper
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


class DeviceUtilsInitTest(unittest.TestCase):

  def testInitWithStr(self):
    serial_as_str = str('0123456789abcdef')
    d = device_utils.DeviceUtils('0123456789abcdef')
    self.assertEqual(serial_as_str, d.adb.GetDeviceSerial())

  def testInitWithUnicode(self):
    serial_as_unicode = unicode('fedcba9876543210')
    d = device_utils.DeviceUtils(serial_as_unicode)
    self.assertEqual(serial_as_unicode, d.adb.GetDeviceSerial())

  def testInitWithAdbWrapper(self):
    serial = '123456789abcdef0'
    a = adb_wrapper.AdbWrapper(serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.adb.GetDeviceSerial())

  def testInitWithAndroidCommands(self):
    serial = '0fedcba987654321'
    a = android_commands.AndroidCommands(device=serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.adb.GetDeviceSerial())

  def testInitWithMissing_fails(self):
    with self.assertRaises(ValueError):
      device_utils.DeviceUtils(None)
    with self.assertRaises(ValueError):
      device_utils.DeviceUtils('')


class DeviceUtilsGetAVDsTest(mock_calls.TestCase):

  def testGetAVDs(self):
    with self.assertCall(
        mock.call.pylib.cmd_helper.GetCmdOutput([mock.ANY, 'list', 'avd']),
        'Available Android Virtual Devices:\n'
        '    Name: my_android5.0\n'
        '    Path: /some/path/to/.android/avd/my_android5.0.avd\n'
        '  Target: Android 5.0 (API level 21)\n'
        ' Tag/ABI: default/x86\n'
        '    Skin: WVGA800\n'):
      self.assertEquals(['my_android5.0'],
                        device_utils.GetAVDs())


class DeviceUtilsRestartServerTest(mock_calls.TestCase):

  @mock.patch('time.sleep', mock.Mock())
  def testRestartServer_succeeds(self):
    with self.assertCalls(
        mock.call.pylib.device.adb_wrapper.AdbWrapper.KillServer(),
        (mock.call.pylib.cmd_helper.GetCmdStatusAndOutput(['pgrep', 'adb']),
         (1, '')),
        mock.call.pylib.device.adb_wrapper.AdbWrapper.StartServer(),
        (mock.call.pylib.cmd_helper.GetCmdStatusAndOutput(['pgrep', 'adb']),
         (1, '')),
        (mock.call.pylib.cmd_helper.GetCmdStatusAndOutput(['pgrep', 'adb']),
         (0, '123\n'))):
      device_utils.RestartServer()


class MockTempFile(object):

  def __init__(self, name='/tmp/some/file'):
    self.file = mock.MagicMock(spec=file)
    self.file.name = name
    self.file.name_quoted = cmd_helper.SingleQuote(name)

  def __enter__(self):
    return self.file

  def __exit__(self, exc_type, exc_val, exc_tb):
    pass


class _PatchedFunction(object):
  def __init__(self, patched=None, mocked=None):
    self.patched = patched
    self.mocked = mocked


def _AdbWrapperMock(test_serial):
  adb = mock.Mock(spec=adb_wrapper.AdbWrapper)
  adb.__str__ = mock.Mock(return_value=test_serial)
  adb.GetDeviceSerial.return_value = test_serial
  return adb


class DeviceUtilsTest(mock_calls.TestCase):

  def setUp(self):
    self.adb = _AdbWrapperMock('0123456789abcdef')
    self.device = device_utils.DeviceUtils(
        self.adb, default_timeout=10, default_retries=0)
    self.watchMethodCalls(self.call.adb, ignore=['GetDeviceSerial'])

  def ShellError(self, output=None, status=1):
    def action(cmd, *args, **kwargs):
      raise device_errors.AdbShellCommandFailedError(
          cmd, output, status, str(self.device))
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


class DeviceUtilsIsOnlineTest(DeviceUtilsTest):

  def testIsOnline_true(self):
    with self.assertCall(self.call.adb.GetState(), 'device'):
      self.assertTrue(self.device.IsOnline())

  def testIsOnline_false(self):
    with self.assertCall(self.call.adb.GetState(), 'offline'):
      self.assertFalse(self.device.IsOnline())

  def testIsOnline_error(self):
    with self.assertCall(self.call.adb.GetState(), self.CommandError()):
      self.assertFalse(self.device.IsOnline())


class DeviceUtilsHasRootTest(DeviceUtilsTest):

  def testHasRoot_true(self):
    with self.assertCall(self.call.adb.Shell('ls /root'), 'foo\n'):
      self.assertTrue(self.device.HasRoot())

  def testHasRoot_false(self):
    with self.assertCall(self.call.adb.Shell('ls /root'), self.ShellError()):
      self.assertFalse(self.device.HasRoot())


class DeviceUtilsEnableRootTest(DeviceUtilsTest):

  def testEnableRoot_succeeds(self):
    with self.assertCalls(
        (self.call.device.IsUserBuild(), False),
        self.call.adb.Root(),
        self.call.adb.WaitForDevice()):
      self.device.EnableRoot()

  def testEnableRoot_userBuild(self):
    with self.assertCalls(
        (self.call.device.IsUserBuild(), True)):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()

  def testEnableRoot_rootFails(self):
    with self.assertCalls(
        (self.call.device.IsUserBuild(), False),
        (self.call.adb.Root(), self.CommandError())):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.EnableRoot()


class DeviceUtilsIsUserBuildTest(DeviceUtilsTest):

  def testIsUserBuild_yes(self):
    with self.assertCall(
        self.call.device.GetProp('ro.build.type', cache=True), 'user'):
      self.assertTrue(self.device.IsUserBuild())

  def testIsUserBuild_no(self):
    with self.assertCall(
        self.call.device.GetProp('ro.build.type', cache=True), 'userdebug'):
      self.assertFalse(self.device.IsUserBuild())


class DeviceUtilsGetExternalStoragePathTest(DeviceUtilsTest):

  def testGetExternalStoragePath_succeeds(self):
    with self.assertCall(
        self.call.adb.Shell('echo $EXTERNAL_STORAGE'), '/fake/storage/path\n'):
      self.assertEquals('/fake/storage/path',
                        self.device.GetExternalStoragePath())

  def testGetExternalStoragePath_fails(self):
    with self.assertCall(self.call.adb.Shell('echo $EXTERNAL_STORAGE'), '\n'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetExternalStoragePath()


class DeviceUtilsGetApplicationPathTest(DeviceUtilsTest):

  def testGetApplicationPath_exists(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '19\n'),
        (self.call.adb.Shell('pm path android'),
         'package:/path/to/android.apk\n')):
      self.assertEquals('/path/to/android.apk',
                        self.device.GetApplicationPath('android'))

  def testGetApplicationPath_notExists(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '19\n'),
        (self.call.adb.Shell('pm path not.installed.app'), '')):
      self.assertEquals(None,
                        self.device.GetApplicationPath('not.installed.app'))

  def testGetApplicationPath_fails(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '19\n'),
        (self.call.adb.Shell('pm path android'),
         self.CommandError('ERROR. Is package manager running?\n'))):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.GetApplicationPath('android')


@mock.patch('time.sleep', mock.Mock())
class DeviceUtilsWaitUntilFullyBootedTest(DeviceUtilsTest):

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
class DeviceUtilsRebootTest(DeviceUtilsTest):

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
        self.call.device.WaitUntilFullyBooted(wifi=False)):
      self.device.Reboot(block=True)

  def testReboot_blockUntilWifi(self):
    with self.assertCalls(
        self.call.adb.Reboot(),
        (self.call.device.IsOnline(), True),
        (self.call.device.IsOnline(), False),
        self.call.device.WaitUntilFullyBooted(wifi=True)):
      self.device.Reboot(block=True, wifi=True)


class DeviceUtilsInstallTest(DeviceUtilsTest):

  def testInstall_noPriorInstall(self):
    with self.assertCalls(
        (mock.call.pylib.utils.apk_helper.GetPackageName('/fake/test/app.apk'),
         'this.is.a.test.package'),
        (self.call.device.GetApplicationPath('this.is.a.test.package'), None),
        self.call.adb.Install('/fake/test/app.apk', reinstall=False)):
      self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_differentPriorInstall(self):
    with self.assertCalls(
        (mock.call.pylib.utils.apk_helper.GetPackageName('/fake/test/app.apk'),
         'this.is.a.test.package'),
        (self.call.device.GetApplicationPath('this.is.a.test.package'),
         '/fake/data/app/this.is.a.test.package.apk'),
        (self.call.device._GetChangedFilesImpl(
            '/fake/test/app.apk', '/fake/data/app/this.is.a.test.package.apk'),
         [('/fake/test/app.apk', '/fake/data/app/this.is.a.test.package.apk')]),
        self.call.adb.Uninstall('this.is.a.test.package'),
        self.call.adb.Install('/fake/test/app.apk', reinstall=False)):
      self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_differentPriorInstall_reinstall(self):
    with self.assertCalls(
        (mock.call.pylib.utils.apk_helper.GetPackageName('/fake/test/app.apk'),
         'this.is.a.test.package'),
        (self.call.device.GetApplicationPath('this.is.a.test.package'),
         '/fake/data/app/this.is.a.test.package.apk'),
        (self.call.device._GetChangedFilesImpl(
            '/fake/test/app.apk', '/fake/data/app/this.is.a.test.package.apk'),
         [('/fake/test/app.apk', '/fake/data/app/this.is.a.test.package.apk')]),
        self.call.adb.Install('/fake/test/app.apk', reinstall=True)):
      self.device.Install('/fake/test/app.apk', reinstall=True, retries=0)

  def testInstall_identicalPriorInstall(self):
    with self.assertCalls(
        (mock.call.pylib.utils.apk_helper.GetPackageName('/fake/test/app.apk'),
         'this.is.a.test.package'),
        (self.call.device.GetApplicationPath('this.is.a.test.package'),
         '/fake/data/app/this.is.a.test.package.apk'),
        (self.call.device._GetChangedFilesImpl(
            '/fake/test/app.apk', '/fake/data/app/this.is.a.test.package.apk'),
         [])):
      self.device.Install('/fake/test/app.apk', retries=0)

  def testInstall_fails(self):
    with self.assertCalls(
        (mock.call.pylib.utils.apk_helper.GetPackageName('/fake/test/app.apk'),
         'this.is.a.test.package'),
        (self.call.device.GetApplicationPath('this.is.a.test.package'), None),
        (self.call.adb.Install('/fake/test/app.apk', reinstall=False),
         self.CommandError('Failure\r\n'))):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.Install('/fake/test/app.apk', retries=0)


class DeviceUtilsRunShellCommandTest(DeviceUtilsTest):

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

  def testRunShellCommand_withHugeCmd(self):
    payload = 'hi! ' * 1024
    expected_cmd = "echo '%s'" % payload
    with self.assertCalls(
      (mock.call.pylib.utils.device_temp_file.DeviceTempFile(
          self.adb, suffix='.sh'), MockTempFile('/sdcard/temp-123.sh')),
      self.call.device._WriteFileWithPush('/sdcard/temp-123.sh', expected_cmd),
      (self.call.adb.Shell('sh /sdcard/temp-123.sh'), payload + '\n')):
      self.assertEquals([payload],
                        self.device.RunShellCommand(['echo', payload]))

  def testRunShellCommand_withHugeCmdAmdSU(self):
    payload = 'hi! ' * 1024
    expected_cmd = """su -c sh -c 'echo '"'"'%s'"'"''""" % payload
    with self.assertCalls(
      (self.call.device.NeedsSU(), True),
      (mock.call.pylib.utils.device_temp_file.DeviceTempFile(
          self.adb, suffix='.sh'), MockTempFile('/sdcard/temp-123.sh')),
      self.call.device._WriteFileWithPush('/sdcard/temp-123.sh', expected_cmd),
      (self.call.adb.Shell('sh /sdcard/temp-123.sh'), payload + '\n')):
      self.assertEquals(
          [payload],
          self.device.RunShellCommand(['echo', payload], as_root=True))

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


class DeviceUtilsGetDevicePieWrapper(DeviceUtilsTest):

  def testGetDevicePieWrapper_jb(self):
    with self.assertCall(
        self.call.device.build_version_sdk(),
        constants.ANDROID_SDK_VERSION_CODES.JELLY_BEAN):
      self.assertEqual('', self.device.GetDevicePieWrapper())

  def testGetDevicePieWrapper_ics(self):
    with self.assertCalls(
        (self.call.device.build_version_sdk(),
         constants.ANDROID_SDK_VERSION_CODES.ICE_CREAM_SANDWICH),
        (mock.call.pylib.constants.GetOutDirectory(), '/foo/bar'),
        (mock.call.os.path.exists(mock.ANY), True),
        (self.call.adb.Push(mock.ANY, mock.ANY), '')):
      self.assertNotEqual('', self.device.GetDevicePieWrapper())


@mock.patch('time.sleep', mock.Mock())
class DeviceUtilsKillAllTest(DeviceUtilsTest):

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


class DeviceUtilsStartActivityTest(DeviceUtilsTest):

  def testStartActivity_actionOnly(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_success(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_failure(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main'),
        'Error: Failed to start test activity'):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.StartActivity(test_intent)

  def testStartActivity_blocking(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-W '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent, blocking=True)

  def testStartActivity_withCategory(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                category='android.intent.category.HOME')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-c android.intent.category.HOME '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withMultipleCategories(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                category=['android.intent.category.HOME',
                                          'android.intent.category.BROWSABLE'])
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-c android.intent.category.HOME '
                            '-c android.intent.category.BROWSABLE '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withData(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                data='http://www.google.com/')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-d http://www.google.com/ '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withStringExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': 'test'})
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main '
                            '--es foo test'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withBoolExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': True})
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main '
                            '--ez foo True'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withIntExtra(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                extras={'foo': 123})
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main '
                            '--ei foo 123'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)

  def testStartActivity_withTraceFile(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '--start-profiler test_trace_file.out '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent,
                                trace_file_name='test_trace_file.out')

  def testStartActivity_withForceStop(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-S '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent, force_stop=True)

  def testStartActivity_withFlags(self):
    test_intent = intent.Intent(action='android.intent.action.VIEW',
                                package='this.is.a.test.package',
                                activity='.Main',
                                flags='0x10000000')
    with self.assertCall(
        self.call.adb.Shell('am start '
                            '-a android.intent.action.VIEW '
                            '-n this.is.a.test.package/.Main '
                            '-f 0x10000000'),
        'Starting: Intent { act=android.intent.action.VIEW }'):
      self.device.StartActivity(test_intent)


class DeviceUtilsStartInstrumentationTest(DeviceUtilsTest):

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


class DeviceUtilsBroadcastIntentTest(DeviceUtilsTest):

  def testBroadcastIntent_noExtras(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT')
    with self.assertCall(
        self.call.adb.Shell('am broadcast -a test.package.with.an.INTENT'),
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)

  def testBroadcastIntent_withExtra(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT',
                                extras={'foo': 'bar value'})
    with self.assertCall(
        self.call.adb.Shell(
            "am broadcast -a test.package.with.an.INTENT --es foo 'bar value'"),
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)

  def testBroadcastIntent_withExtra_noValue(self):
    test_intent = intent.Intent(action='test.package.with.an.INTENT',
                                extras={'foo': None})
    with self.assertCall(
        self.call.adb.Shell(
            'am broadcast -a test.package.with.an.INTENT --esn foo'),
        'Broadcasting: Intent { act=test.package.with.an.INTENT } '):
      self.device.BroadcastIntent(test_intent)


class DeviceUtilsGoHomeTest(DeviceUtilsTest):

  def testGoHome(self):
    with self.assertCall(
        self.call.adb.Shell('am start -W -a android.intent.action.MAIN '
                            '-c android.intent.category.HOME'),
        'Starting: Intent { act=android.intent.action.MAIN }\r\n'):
      self.device.GoHome()


class DeviceUtilsForceStopTest(DeviceUtilsTest):

  def testForceStop(self):
    with self.assertCall(
        self.call.adb.Shell('am force-stop this.is.a.test.package'),
        ''):
      self.device.ForceStop('this.is.a.test.package')


class DeviceUtilsClearApplicationStateTest(DeviceUtilsTest):

  def testClearApplicationState_packageDoesntExist(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '17\n'),
        (self.call.device.GetApplicationPath('this.package.does.not.exist'),
         None)):
      self.device.ClearApplicationState('this.package.does.not.exist')

  def testClearApplicationState_packageDoesntExistOnAndroidJBMR2OrAbove(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '18\n'),
        (self.call.adb.Shell('pm clear this.package.does.not.exist'),
         'Failed\r\n')):
      self.device.ClearApplicationState('this.package.does.not.exist')

  def testClearApplicationState_packageExists(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '17\n'),
        (self.call.device.GetApplicationPath('this.package.exists'),
         '/data/app/this.package.exists.apk'),
        (self.call.adb.Shell('pm clear this.package.exists'),
         'Success\r\n')):
      self.device.ClearApplicationState('this.package.exists')

  def testClearApplicationState_packageExistsOnAndroidJBMR2OrAbove(self):
    with self.assertCalls(
        (self.call.adb.Shell('getprop ro.build.version.sdk'), '18\n'),
        (self.call.adb.Shell('pm clear this.package.exists'),
         'Success\r\n')):
      self.device.ClearApplicationState('this.package.exists')


class DeviceUtilsSendKeyEventTest(DeviceUtilsTest):

  def testSendKeyEvent(self):
    with self.assertCall(self.call.adb.Shell('input keyevent 66'), ''):
      self.device.SendKeyEvent(66)


class DeviceUtilsPushChangedFilesIndividuallyTest(DeviceUtilsTest):

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


class DeviceUtilsPushChangedFilesZippedTest(DeviceUtilsTest):

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
            env={'PATH': '/data/local/tmp/bin:$PATH'},
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


class DeviceUtilsFileExistsTest(DeviceUtilsTest):

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


class DeviceUtilsPullFileTest(DeviceUtilsTest):

  def testPullFile_existsOnDevice(self):
    with mock.patch('os.path.exists', return_value=True):
      with self.assertCall(
          self.call.adb.Pull('/data/app/test.file.exists',
                             '/test/file/host/path')):
        self.device.PullFile('/data/app/test.file.exists',
                             '/test/file/host/path')

  def testPullFile_doesntExistOnDevice(self):
    with mock.patch('os.path.exists', return_value=True):
      with self.assertCall(
          self.call.adb.Pull('/data/app/test.file.does.not.exist',
                             '/test/file/host/path'),
          self.CommandError('remote object does not exist')):
        with self.assertRaises(device_errors.CommandFailedError):
          self.device.PullFile('/data/app/test.file.does.not.exist',
                               '/test/file/host/path')


class DeviceUtilsReadFileTest(DeviceUtilsTest):

  def testReadFileWithPull_success(self):
    tmp_host_dir = '/tmp/dir/on.host/'
    tmp_host = MockTempFile('/tmp/dir/on.host/tmp_ReadFileWithPull')
    tmp_host.file.read.return_value = 'some interesting contents'
    with self.assertCalls(
        (mock.call.tempfile.mkdtemp(), tmp_host_dir),
        (self.call.adb.Pull('/path/to/device/file', mock.ANY)),
        (mock.call.__builtin__.open(mock.ANY, 'r'), tmp_host),
        (mock.call.os.path.exists(tmp_host_dir), True),
        (mock.call.shutil.rmtree(tmp_host_dir), None)):
      self.assertEquals('some interesting contents',
                        self.device._ReadFileWithPull('/path/to/device/file'))
    tmp_host.file.read.assert_called_once_with()

  def testReadFileWithPull_rejected(self):
    tmp_host_dir = '/tmp/dir/on.host/'
    with self.assertCalls(
        (mock.call.tempfile.mkdtemp(), tmp_host_dir),
        (self.call.adb.Pull('/path/to/device/file', mock.ANY),
         self.CommandError()),
        (mock.call.os.path.exists(tmp_host_dir), True),
        (mock.call.shutil.rmtree(tmp_host_dir), None)):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device._ReadFileWithPull('/path/to/device/file')

  def testReadFile_exists(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['ls', '-l', '/read/this/test/file'],
            as_root=False, check_return=True),
         ['-rw-rw---- root foo 256 1970-01-01 00:00 file']),
        (self.call.device.RunShellCommand(
            ['cat', '/read/this/test/file'], as_root=False, check_return=True),
         ['this is a test file'])):
      self.assertEqual('this is a test file\n',
                       self.device.ReadFile('/read/this/test/file'))

  def testReadFile_doesNotExist(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['ls', '-l', '/this/file/does.not.exist'],
            as_root=False, check_return=True),
        self.CommandError('File does not exist')):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.ReadFile('/this/file/does.not.exist')

  def testReadFile_withSU(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['ls', '-l', '/this/file/can.be.read.with.su'],
            as_root=True, check_return=True),
         ['-rw------- root root 256 1970-01-01 00:00 can.be.read.with.su']),
        (self.call.device.RunShellCommand(
            ['cat', '/this/file/can.be.read.with.su'],
            as_root=True, check_return=True),
         ['this is a test file', 'read with su'])):
      self.assertEqual(
          'this is a test file\nread with su\n',
          self.device.ReadFile('/this/file/can.be.read.with.su',
                               as_root=True))

  def testReadFile_withPull(self):
    contents = 'a' * 123456
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['ls', '-l', '/read/this/big/test/file'],
            as_root=False, check_return=True),
         ['-rw-rw---- root foo 123456 1970-01-01 00:00 file']),
        (self.call.device._ReadFileWithPull('/read/this/big/test/file'),
         contents)):
      self.assertEqual(
          contents, self.device.ReadFile('/read/this/big/test/file'))

  def testReadFile_withPullAndSU(self):
    contents = 'b' * 123456
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['ls', '-l', '/this/big/file/can.be.read.with.su'],
            as_root=True, check_return=True),
         ['-rw------- root root 123456 1970-01-01 00:00 can.be.read.with.su']),
        (self.call.device.NeedsSU(), True),
        (mock.call.pylib.utils.device_temp_file.DeviceTempFile(self.adb),
         MockTempFile('/sdcard/tmp/on.device')),
        self.call.device.RunShellCommand(
            ['cp', '/this/big/file/can.be.read.with.su',
             '/sdcard/tmp/on.device'],
            as_root=True, check_return=True),
        (self.call.device._ReadFileWithPull('/sdcard/tmp/on.device'),
         contents)):
      self.assertEqual(
          contents,
          self.device.ReadFile('/this/big/file/can.be.read.with.su',
                               as_root=True))


class DeviceUtilsWriteFileTest(DeviceUtilsTest):

  def testWriteFileWithPush_success(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'some interesting contents'
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        self.call.adb.Push('/tmp/file/on.host', '/path/to/device/file')):
      self.device._WriteFileWithPush('/path/to/device/file', contents)
    tmp_host.file.write.assert_called_once_with(contents)

  def testWriteFileWithPush_rejected(self):
    tmp_host = MockTempFile('/tmp/file/on.host')
    contents = 'some interesting contents'
    with self.assertCalls(
        (mock.call.tempfile.NamedTemporaryFile(), tmp_host),
        (self.call.adb.Push('/tmp/file/on.host', '/path/to/device/file'),
         self.CommandError())):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device._WriteFileWithPush('/path/to/device/file', contents)

  def testWriteFile_withPush(self):
    contents = 'some large contents ' * 26 # 20 * 26 = 520 chars
    with self.assertCalls(
        self.call.device._WriteFileWithPush('/path/to/device/file', contents)):
      self.device.WriteFile('/path/to/device/file', contents)

  def testWriteFile_withPushForced(self):
    contents = 'tiny contents'
    with self.assertCalls(
        self.call.device._WriteFileWithPush('/path/to/device/file', contents)):
      self.device.WriteFile('/path/to/device/file', contents, force_push=True)

  def testWriteFile_withPushAndSU(self):
    contents = 'some large contents ' * 26 # 20 * 26 = 520 chars
    with self.assertCalls(
        (self.call.device.NeedsSU(), True),
        (mock.call.pylib.utils.device_temp_file.DeviceTempFile(self.adb),
         MockTempFile('/sdcard/tmp/on.device')),
        self.call.device._WriteFileWithPush('/sdcard/tmp/on.device', contents),
        self.call.device.RunShellCommand(
            ['cp', '/sdcard/tmp/on.device', '/path/to/device/file'],
            as_root=True, check_return=True)):
      self.device.WriteFile('/path/to/device/file', contents, as_root=True)

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


class DeviceUtilsLsTest(DeviceUtilsTest):

  def testLs_directory(self):
    result = [('.', adb_wrapper.DeviceStat(16889, 4096, 1417436123)),
              ('..', adb_wrapper.DeviceStat(16873, 4096, 12382237)),
              ('testfile.txt', adb_wrapper.DeviceStat(33206, 3, 1417436122))]
    with self.assertCalls(
        (self.call.adb.Ls('/data/local/tmp'), result)):
      self.assertEquals(result,
                        self.device.Ls('/data/local/tmp'))

  def testLs_nothing(self):
    with self.assertCalls(
        (self.call.adb.Ls('/data/local/tmp/testfile.txt'), [])):
      self.assertEquals([],
                        self.device.Ls('/data/local/tmp/testfile.txt'))


class DeviceUtilsStatTest(DeviceUtilsTest):

  def testStat_file(self):
    result = [('.', adb_wrapper.DeviceStat(16889, 4096, 1417436123)),
              ('..', adb_wrapper.DeviceStat(16873, 4096, 12382237)),
              ('testfile.txt', adb_wrapper.DeviceStat(33206, 3, 1417436122))]
    with self.assertCalls(
        (self.call.adb.Ls('/data/local/tmp'), result)):
      self.assertEquals(adb_wrapper.DeviceStat(33206, 3, 1417436122),
                        self.device.Stat('/data/local/tmp/testfile.txt'))

  def testStat_directory(self):
    result = [('.', adb_wrapper.DeviceStat(16873, 4096, 12382237)),
              ('..', adb_wrapper.DeviceStat(16873, 4096, 12382237)),
              ('tmp', adb_wrapper.DeviceStat(16889, 4096, 1417436123))]
    with self.assertCalls(
        (self.call.adb.Ls('/data/local'), result)):
      self.assertEquals(adb_wrapper.DeviceStat(16889, 4096, 1417436123),
                        self.device.Stat('/data/local/tmp'))

  def testStat_doesNotExist(self):
    result = [('.', adb_wrapper.DeviceStat(16889, 4096, 1417436123)),
              ('..', adb_wrapper.DeviceStat(16873, 4096, 12382237)),
              ('testfile.txt', adb_wrapper.DeviceStat(33206, 3, 1417436122))]
    with self.assertCalls(
        (self.call.adb.Ls('/data/local/tmp'), result)):
      with self.assertRaises(device_errors.CommandFailedError):
        self.device.Stat('/data/local/tmp/does.not.exist.txt')


class DeviceUtilsSetJavaAssertsTest(DeviceUtilsTest):

  def testSetJavaAsserts_enable(self):
    with self.assertCalls(
        (self.call.device.ReadFile(constants.DEVICE_LOCAL_PROPERTIES_PATH),
         'some.example.prop=with an example value\n'
         'some.other.prop=value_ok\n'),
        self.call.device.WriteFile(
            constants.DEVICE_LOCAL_PROPERTIES_PATH,
            'some.example.prop=with an example value\n'
            'some.other.prop=value_ok\n'
            'dalvik.vm.enableassertions=all\n'),
        (self.call.device.GetProp('dalvik.vm.enableassertions'), ''),
        self.call.device.SetProp('dalvik.vm.enableassertions', 'all')):
      self.assertTrue(self.device.SetJavaAsserts(True))

  def testSetJavaAsserts_disable(self):
    with self.assertCalls(
        (self.call.device.ReadFile(constants.DEVICE_LOCAL_PROPERTIES_PATH),
         'some.example.prop=with an example value\n'
         'dalvik.vm.enableassertions=all\n'
         'some.other.prop=value_ok\n'),
        self.call.device.WriteFile(
            constants.DEVICE_LOCAL_PROPERTIES_PATH,
            'some.example.prop=with an example value\n'
            'some.other.prop=value_ok\n'),
        (self.call.device.GetProp('dalvik.vm.enableassertions'), 'all'),
        self.call.device.SetProp('dalvik.vm.enableassertions', '')):
      self.assertTrue(self.device.SetJavaAsserts(False))

  def testSetJavaAsserts_alreadyEnabled(self):
    with self.assertCalls(
        (self.call.device.ReadFile(constants.DEVICE_LOCAL_PROPERTIES_PATH),
         'some.example.prop=with an example value\n'
         'dalvik.vm.enableassertions=all\n'
         'some.other.prop=value_ok\n'),
        (self.call.device.GetProp('dalvik.vm.enableassertions'), 'all')):
      self.assertFalse(self.device.SetJavaAsserts(True))


class DeviceUtilsGetPropTest(DeviceUtilsTest):

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


class DeviceUtilsSetPropTest(DeviceUtilsTest):

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


class DeviceUtilsGetPidsTest(DeviceUtilsTest):

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


class DeviceUtilsTakeScreenshotTest(DeviceUtilsTest):

  def testTakeScreenshot_fileNameProvided(self):
    with self.assertCalls(
        (mock.call.pylib.utils.device_temp_file.DeviceTempFile(
            self.adb, suffix='.png'),
         MockTempFile('/tmp/path/temp-123.png')),
        (self.call.adb.Shell('/system/bin/screencap -p /tmp/path/temp-123.png'),
         ''),
        self.call.device.PullFile('/tmp/path/temp-123.png',
                                  '/test/host/screenshot.png')):
      self.device.TakeScreenshot('/test/host/screenshot.png')


class DeviceUtilsGetMemoryUsageForPidTest(DeviceUtilsTest):

  def setUp(self):
    super(DeviceUtilsGetMemoryUsageForPidTest, self).setUp()

  def testGetMemoryUsageForPid_validPid(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['showmap', '1234'], as_root=True, check_return=True),
         ['100 101 102 103 104 105 106 107 TOTAL']),
        (self.call.device.ReadFile('/proc/1234/status', as_root=True),
         'VmHWM: 1024 kB\n')):
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

  def testGetMemoryUsageForPid_noSmaps(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['showmap', '4321'], as_root=True, check_return=True),
         ['cannot open /proc/4321/smaps: No such file or directory']),
        (self.call.device.ReadFile('/proc/4321/status', as_root=True),
         'VmHWM: 1024 kb\n')):
      self.assertEquals({'VmHWM': 1024}, self.device.GetMemoryUsageForPid(4321))

  def testGetMemoryUsageForPid_noStatus(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['showmap', '4321'], as_root=True, check_return=True),
         ['100 101 102 103 104 105 106 107 TOTAL']),
        (self.call.device.ReadFile('/proc/4321/status', as_root=True),
         self.CommandError())):
      self.assertEquals(
          {
            'Size': 100,
            'Rss': 101,
            'Pss': 102,
            'Shared_Clean': 103,
            'Shared_Dirty': 104,
            'Private_Clean': 105,
            'Private_Dirty': 106,
          },
          self.device.GetMemoryUsageForPid(4321))


class DeviceUtilsGetBatteryInfoTest(DeviceUtilsTest):
  def testGetBatteryInfo_normal(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'battery'], check_return=True),
        [
          'Current Battery Service state:',
          '  AC powered: false',
          '  USB powered: true',
          '  level: 100',
          '  temperature: 321',
        ]):
      self.assertEquals(
          {
            'AC powered': 'false',
            'USB powered': 'true',
            'level': '100',
            'temperature': '321',
          },
          self.device.GetBatteryInfo())


  def testGetBatteryInfo_nothing(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'battery'], check_return=True), []):
      self.assertEquals({}, self.device.GetBatteryInfo())


class DeviceUtilsGetChargingTest(DeviceUtilsTest):
  def testGetCharging_usb(self):
    with self.assertCall(
        self.call.device.GetBatteryInfo(), {'USB powered': 'true'}):
      self.assertTrue(self.device.GetCharging())

  def testGetCharging_usbFalse(self):
    with self.assertCall(
        self.call.device.GetBatteryInfo(), {'USB powered': 'false'}):
      self.assertFalse(self.device.GetCharging())

  def testGetCharging_ac(self):
    with self.assertCall(
        self.call.device.GetBatteryInfo(), {'AC powered': 'true'}):
      self.assertTrue(self.device.GetCharging())

  def testGetCharging_wireless(self):
    with self.assertCall(
        self.call.device.GetBatteryInfo(), {'Wireless powered': 'true'}):
      self.assertTrue(self.device.GetCharging())

  def testGetCharging_unknown(self):
    with self.assertCall(
        self.call.device.GetBatteryInfo(), {'level': '42'}):
      self.assertFalse(self.device.GetCharging())


class DeviceUtilsSetChargingTest(DeviceUtilsTest):

  @mock.patch('time.sleep', mock.Mock())
  def testSetCharging_enabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), False),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True)):
      self.device.SetCharging(True)

  def testSetCharging_alreadyEnabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True)):
      self.device.SetCharging(True)

  @mock.patch('time.sleep', mock.Mock())
  def testSetCharging_disabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), False)):
      self.device.SetCharging(False)


class DeviceUtilsSetBatteryMeasurementTest(DeviceUtilsTest):

  def testBatteryMeasurement(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            mock.ANY, retries=0, single_line=True,
            timeout=10, check_return=True), '22'),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--reset'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--charged', '--checkin'],
            check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'usb', '0'], check_return=True), []),
        (self.call.device.GetCharging(), False),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'usb', '1'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'reset'], check_return=True), []),
        (self.call.device.GetCharging(), True)):
      with self.device.BatteryMeasurement():
        pass


class DeviceUtilsStrTest(DeviceUtilsTest):

  def testStr_returnsSerial(self):
    with self.assertCalls(
        (self.call.adb.GetDeviceSerial(), '0123456789abcdef')):
      self.assertEqual('0123456789abcdef', str(self.device))


class DeviceUtilsParallelTest(mock_calls.TestCase):

  def testParallel_default(self):
    test_serials = ['0123456789abcdef', 'fedcba9876543210']
    with self.assertCall(
        mock.call.pylib.device.adb_wrapper.AdbWrapper.GetDevices(),
        [_AdbWrapperMock(serial) for serial in test_serials]):
      parallel_devices = device_utils.DeviceUtils.parallel()
    for serial, device in zip(test_serials, parallel_devices.pGet(None)):
      self.assertTrue(
          isinstance(device, device_utils.DeviceUtils)
          and serial == str(device),
          'Expected a DeviceUtils object with serial %s' % serial)

  def testParallel_noDevices(self):
    with self.assertCall(
        mock.call.pylib.device.adb_wrapper.AdbWrapper.GetDevices(), []):
      with self.assertRaises(device_errors.NoDevicesError):
        device_utils.DeviceUtils.parallel()


class DeviceUtilsClientCache(DeviceUtilsTest):

  def testClientCache_twoCaches(self):
    self.device._cache['test'] = 0
    client_cache_one = self.device.GetClientCache('ClientOne')
    client_cache_one['test'] = 1
    client_cache_two = self.device.GetClientCache('ClientTwo')
    client_cache_two['test'] = 2
    self.assertEqual(self.device._cache, {'test': 0})
    self.assertEqual(client_cache_one, {'test': 1})
    self.assertEqual(client_cache_two, {'test': 2})
    self.device._ClearCache()
    self.assertEqual(self.device._cache, {})
    self.assertEqual(client_cache_one, {})
    self.assertEqual(client_cache_two, {})

  def testClientCache_multipleInstances(self):
    client_cache_one = self.device.GetClientCache('ClientOne')
    client_cache_one['test'] = 1
    client_cache_two = self.device.GetClientCache('ClientOne')
    self.assertEqual(client_cache_one, {'test': 1})
    self.assertEqual(client_cache_two, {'test': 1})
    self.device._ClearCache()
    self.assertEqual(client_cache_one, {})
    self.assertEqual(client_cache_two, {})

if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)

