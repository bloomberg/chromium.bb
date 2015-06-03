# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the remote_access module."""

from __future__ import print_function

import os
import re
import socket

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import debug_link
from chromite.lib import mdns
from chromite.lib import mdns_unittest
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import remote_access


# pylint: disable=protected-access


class TestNormalizePort(cros_test_lib.TestCase):
  """Verifies we normalize port."""

  def testNormalizePortStrOK(self):
    """Tests that string will be converted to integer."""
    self.assertEqual(remote_access.NormalizePort('123'), 123)

  def testNormalizePortStrNotOK(self):
    """Tests that error is raised if port is string and str_ok=False."""
    self.assertRaises(
        ValueError, remote_access.NormalizePort, '123', str_ok=False)

  def testNormalizePortOutOfRange(self):
    """Tests that error is rasied when port is out of range."""
    self.assertRaises(ValueError, remote_access.NormalizePort, '-1')
    self.assertRaises(ValueError, remote_access.NormalizePort, 99999)


class TestRemoveKnownHost(cros_test_lib.MockTempDirTestCase):
  """Verifies RemoveKnownHost() functionality."""

  # ssh-keygen doesn't check for a valid hostname so use something that won't
  # be in the user's known_hosts to avoid changing their file contents.
  _HOST = '0.0.0.0.0.0'

  _HOST_KEY = (
      _HOST + ' ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCjysPTaDAtRaxRaW1JjqzCHp2'
      '88gvlUgtJxd2Jt/v63fkqZ5zzLLoeoAMwv0oYSRU82qhLimXpHxXRkrMC5nrpz5zJch+ktql'
      '0rSRgo+dqc1GzmyOOAq5NkQsgBb3hefxMxCZRV8Dv0n7qaindZRxE8MnRJmVUoj8Wq8wryab'
      'p+fUBkesBwaJhPXa4WBJeI5d+rO5tEBSNkvIp0USU6Ku3Ct0q2sZbOkY5g1VFAUYm4wyshCf'
      'oWvU8ivMFp0pCezMISGstKpkIQApq2dLUb6EmeIgnhHzZXOn7doxIGD33JUfFmwNi0qfk3vV'
      '6vKRVDEZD68+ix6gjKpicY5upA/9P\n')

  def testRemoveKnownHostDefaultFile(self):
    """Tests RemoveKnownHost() on the default known_hosts file.

    `ssh-keygen -R` on its own fails when run from within the chroot
    since the default known_hosts is bind mounted.
    """
    # It doesn't matter if known_hosts actually has this host in it or not,
    # this test just makes sure the command doesn't fail. The default
    # known_hosts file always exists in the chroot due to the bind mount.
    remote_access.RemoveKnownHost(self._HOST)

  def testRemoveKnownHostCustomFile(self):
    """Tests RemoveKnownHost() on a custom known_hosts file."""
    path = os.path.join(self.tempdir, 'known_hosts')
    osutils.WriteFile(path, self._HOST_KEY)
    remote_access.RemoveKnownHost(self._HOST, known_hosts_path=path)
    self.assertEqual(osutils.ReadFile(path), '')

  def testRemoveKnownHostNonexistentFile(self):
    """Tests RemoveKnownHost() on a nonexistent known_hosts file."""
    path = os.path.join(self.tempdir, 'known_hosts')
    remote_access.RemoveKnownHost(self._HOST, known_hosts_path=path)


class TestCompileSSHConnectSettings(cros_test_lib.TestCase):
  """Verifies CompileSSHConnectSettings()."""

  def testCustomSettingIncluded(self):
    """Tests that a custom setting will be included in the output."""
    self.assertIn(
        '-oNumberOfPasswordPrompts=100',
        remote_access.CompileSSHConnectSettings(NumberOfPasswordPrompts=100))

  def testNoneSettingOmitted(self):
    """Tests that a None value will omit a default setting from the output."""
    self.assertIn('-oProtocol=2', remote_access.CompileSSHConnectSettings())
    self.assertNotIn(
        '-oProtocol=2',
        remote_access.CompileSSHConnectSettings(Protocol=None))


class RemoteShMock(partial_mock.PartialCmdMock):
  """Mocks the RemoteSh function."""
  TARGET = 'chromite.lib.remote_access.RemoteAccess'
  ATTRS = ('RemoteSh',)
  DEFAULT_ATTR = 'RemoteSh'

  def RemoteSh(self, inst, cmd, *args, **kwargs):
    """Simulates a RemoteSh invocation.

    Returns:
      A CommandResult object with an additional member |rc_mock| to
      enable examination of the underlying RunCommand() function call.
    """
    result = self._results['RemoteSh'].LookupResult(
        (cmd,), hook_args=(inst, cmd,) + args, hook_kwargs=kwargs)

    # Run the real RemoteSh with RunCommand mocked out.
    rc_mock = cros_build_lib_unittest.RunCommandMock()
    rc_mock.AddCmdResult(
        partial_mock.Ignore(), result.returncode, result.output, result.error)

    with rc_mock:
      result = self.backup['RemoteSh'](inst, cmd, *args, **kwargs)
    result.rc_mock = rc_mock
    return result


class RemoteDeviceMock(partial_mock.PartialMock):
  """Mocks the RemoteDevice function."""

  TARGET = 'chromite.lib.remote_access.RemoteDevice'
  ATTRS = ('Pingable',)

  def Pingable(self, _):
    return True


class RemoteAccessTest(cros_test_lib.MockTempDirTestCase):
  """Base class with RemoteSh mocked out for testing RemoteAccess."""
  def setUp(self):
    self.rsh_mock = self.StartPatcher(RemoteShMock())
    self.host = remote_access.RemoteAccess('foon', self.tempdir)


class RemoteShTest(RemoteAccessTest):
  """Tests of basic RemoteSh functions"""
  TEST_CMD = 'ls'
  RETURN_CODE = 0
  OUTPUT = 'witty'
  ERROR = 'error'

  def assertRemoteShRaises(self, **kwargs):
    """Asserts that RunCommandError is raised when running TEST_CMD."""
    self.assertRaises(cros_build_lib.RunCommandError, self.host.RemoteSh,
                      self.TEST_CMD, **kwargs)

  def assertRemoteShRaisesSSHConnectionError(self, **kwargs):
    """Asserts that SSHConnectionError is raised when running TEST_CMD."""
    self.assertRaises(remote_access.SSHConnectionError, self.host.RemoteSh,
                      self.TEST_CMD, **kwargs)

  def SetRemoteShResult(self, returncode=RETURN_CODE, output=OUTPUT,
                        error=ERROR):
    """Sets the RemoteSh command results."""
    self.rsh_mock.AddCmdResult(self.TEST_CMD, returncode=returncode,
                               output=output, error=error)

  def testNormal(self):
    """Test normal functionality."""
    self.SetRemoteShResult()
    result = self.host.RemoteSh(self.TEST_CMD)
    self.assertEquals(result.returncode, self.RETURN_CODE)
    self.assertEquals(result.output.strip(), self.OUTPUT)
    self.assertEquals(result.error.strip(), self.ERROR)

  def testRemoteCmdFailure(self):
    """Test failure in remote cmd."""
    self.SetRemoteShResult(returncode=1)
    self.assertRemoteShRaises()
    self.assertRemoteShRaises(ssh_error_ok=True)
    self.host.RemoteSh(self.TEST_CMD, error_code_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True, error_code_ok=True)

  def testSshFailure(self):
    """Test failure in ssh command."""
    self.SetRemoteShResult(returncode=remote_access.SSH_ERROR_CODE)
    self.assertRemoteShRaisesSSHConnectionError()
    self.assertRemoteShRaisesSSHConnectionError(error_code_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True, error_code_ok=True)

  def testEnvLcMessagesSet(self):
    """Test that LC_MESSAGES is set to 'C' for an SSH command."""
    self.SetRemoteShResult()
    result = self.host.RemoteSh(self.TEST_CMD)
    rc_kwargs = result.rc_mock.call_args_list[-1][1]
    self.assertEqual(rc_kwargs['extra_env']['LC_MESSAGES'], 'C')

  def testEnvLcMessagesOverride(self):
    """Test that LC_MESSAGES is overridden to 'C' for an SSH command."""
    self.SetRemoteShResult()
    result = self.host.RemoteSh(self.TEST_CMD, extra_env={'LC_MESSAGES': 'fr'})
    rc_kwargs = result.rc_mock.call_args_list[-1][1]
    self.assertEqual(rc_kwargs['extra_env']['LC_MESSAGES'], 'C')


class CheckIfRebootedTest(RemoteAccessTest):
  """Tests of the _CheckIfRebooted function."""

  def MockCheckReboot(self, returncode):
    self.rsh_mock.AddCmdResult(
        partial_mock.Regex('.*%s.*' % re.escape(remote_access.REBOOT_MARKER)),
        returncode)

  def testSuccess(self):
    """Test the case of successful reboot."""
    self.MockCheckReboot(0)
    self.assertTrue(self.host._CheckIfRebooted())

  def testRemoteFailure(self):
    """Test case of reboot pending."""
    self.MockCheckReboot(1)
    self.assertFalse(self.host._CheckIfRebooted())

  def testSshFailure(self):
    """Test case of connection down."""
    self.MockCheckReboot(remote_access.SSH_ERROR_CODE)
    self.assertFalse(self.host._CheckIfRebooted())

  def testInvalidErrorCode(self):
    """Test case of bad error code returned."""
    self.MockCheckReboot(2)
    self.assertRaises(Exception, self.host._CheckIfRebooted)


class RemoteDeviceTest(cros_test_lib.MockTestCase):
  """Tests for RemoteDevice class."""

  def setUp(self):
    self.rsh_mock = self.StartPatcher(RemoteShMock())
    self.pingable_mock = self.PatchObject(
        remote_access.RemoteDevice, 'Pingable', return_value=True)

  def _SetupRemoteTempDir(self):
    """Mock out the calls needed for a remote tempdir."""
    self.rsh_mock.AddCmdResult(partial_mock.In('mktemp'))
    self.rsh_mock.AddCmdResult(partial_mock.In('rm'))

  def testCommands(self):
    """Tests simple RunCommand() and BaseRunCommand() usage."""
    command = ['echo', 'foo']
    expected_output = 'foo'
    self.rsh_mock.AddCmdResult(command, output=expected_output)
    self._SetupRemoteTempDir()

    with remote_access.RemoteDeviceHandler('1.1.1.1') as device:
      self.assertEqual(expected_output,
                       device.RunCommand(['echo', 'foo']).output)
      self.assertEqual(expected_output,
                       device.BaseRunCommand(['echo', 'foo']).output)

  def testRunCommandShortCmdline(self):
    """Verify short command lines execute env settings directly."""
    with remote_access.RemoteDeviceHandler('1.1.1.1') as device:
      self.PatchObject(remote_access.RemoteDevice, 'CopyToWorkDir',
                       side_effect=Exception('should not be copying files'))
      self.rsh_mock.AddCmdResult(partial_mock.In('runit'))
      device.RunCommand(['runit'], extra_env={'VAR': 'val'})

  def testRunCommandLongCmdline(self):
    """Verify long command lines execute env settings via script."""
    with remote_access.RemoteDeviceHandler('1.1.1.1') as device:
      self._SetupRemoteTempDir()
      m = self.PatchObject(remote_access.RemoteDevice, 'CopyToWorkDir')
      self.rsh_mock.AddCmdResult(partial_mock.In('runit'))
      device.RunCommand(['runit'], extra_env={'VAR': 'v' * 1024 * 1024})
      # We'll assume that the test passed when it tries to copy a file to the
      # remote side (the shell script to run indirectly).
      self.assertEqual(m.call_count, 1)

  def testNoDeviceBaseDir(self):
    """Tests base_dir=None."""
    command = ['echo', 'foo']
    expected_output = 'foo'
    self.rsh_mock.AddCmdResult(command, output=expected_output)

    with remote_access.RemoteDeviceHandler('1.1.1.1', base_dir=None) as device:
      self.assertEqual(expected_output,
                       device.BaseRunCommand(['echo', 'foo']).output)

  def testDelayedRemoteDirs(self):
    """Tests the delayed creation of base_dir/work_dir."""
    with remote_access.RemoteDeviceHandler('1.1.1.1', base_dir='/f') as device:
      # Make sure we didn't talk to the remote yet.
      self.assertEqual(self.rsh_mock.call_count, 0)

      # The work dir will get automatically created when we use it.
      self.rsh_mock.AddCmdResult(partial_mock.In('mktemp'))
      _ = device.work_dir
      self.assertEqual(self.rsh_mock.call_count, 1)

      # Add a mock for the clean up logic.
      self.rsh_mock.AddCmdResult(partial_mock.In('rm'))

    self.assertEqual(self.rsh_mock.call_count, 2)


class USBDeviceTestCase(mdns_unittest.mDnsTestCase):
  """Base class for USB device related tests."""

  def setUp(self):
    self.StartPatcher(RemoteDeviceMock())
    self.initializedebuglink_mock = self.PatchObject(debug_link,
                                                     'InitializeDebugLink')


class TestGetUSBConnectedDevices(USBDeviceTestCase):
  """Tests of the GetUSBConnectedDevices() function."""

  def testDebugLinkInitialization(self):
    """Test case to make sure the Debug Link is initialized."""
    self.PatchObject(mdns, 'FindServices')
    remote_access.GetUSBConnectedDevices()
    self.initializedebuglink_mock.assert_called_once()

  def testEnumeration(self):
    """Test case to check correct enumeration results."""
    services = [
        mdns.Service('d1.local', '1.1.1.1', 0, 'd1.a.local', {'alias': 'd1'}),
        mdns.Service('d2.local', '2.2.2.2', 0, 'd2.a.local', {'alias': 'd2'})]
    self._MockNetworkResponse(services)
    devices = remote_access.GetUSBConnectedDevices()
    self.assertEqual(len(devices), len(services))
    for index in range(len(devices)):
      self.assertEqual(devices[index].hostname, services[index].ip)
      self.assertEqual(devices[index].alias, services[index].text['alias'])


class TestGetDefaultService(USBDeviceTestCase):
  """Tests _GetDefaultService() function."""

  SERVICE_1 = mdns.Service('d1.local', '1.1.1.1', 0, 'd1.a.local',
                           {'alias': 'd1'})
  SERVICE_2 = mdns.Service('d2.local', '2.2.2.2', 0, 'd2.a.local',
                           {'alias': 'd2'})

  def testNoDevices(self):
    """Tests when no devices are found."""
    self._MockNetworkResponse([])
    with self.assertRaises(remote_access.DefaultDeviceError):
      remote_access._GetDefaultService()

  def testOneDevice(self):
    """Tests when one device is found."""
    self._MockNetworkResponse([self.SERVICE_1])
    service, connection_type = remote_access._GetDefaultService()
    self.assertEqual(self.SERVICE_1.ip, service.ip)
    self.assertEqual(
        self.SERVICE_1.text[remote_access.BRILLO_DEVICE_PROPERTY_ALIAS],
        service.text[remote_access.BRILLO_DEVICE_PROPERTY_ALIAS])
    self.assertEqual(remote_access.CONNECTION_TYPE_USB, connection_type)

  def testMultipleDevices(self):
    """Tests when multiple devices are found."""
    self._MockNetworkResponse([self.SERVICE_1, self.SERVICE_2])
    with self.assertRaises(remote_access.DefaultDeviceError):
      remote_access._GetDefaultService()

  def testDefaultChromiumOSDevice(self):
    """Tests finding the default ChromiumOSDevice."""
    self._MockNetworkResponse([self.SERVICE_1])
    device = remote_access.ChromiumOSDevice(None, ping=False, connect=False)
    self.assertEqual(self.SERVICE_1.ip, device.hostname)
    self.assertEqual(
        self.SERVICE_1.text[remote_access.BRILLO_DEVICE_PROPERTY_ALIAS],
        device.alias)
    self.assertEqual(remote_access.CONNECTION_TYPE_USB, device.connection_type)


class TestUSBDeviceIP(USBDeviceTestCase):
  """Tests of the GetUSBDeviceIP() function."""

  def testEmptyAlias(self):
    """Test empty alias should resolve to None."""
    ip = remote_access.GetUSBDeviceIP('')
    self.assertEqual(ip, None)

  def testDebugLinkInitialization(self):
    """Test case to make sure the Debug Link is initialized."""
    self.PatchObject(mdns, 'FindServices')
    remote_access.GetUSBDeviceIP('dut')
    self.initializedebuglink_mock.assert_called_once()

  def testSuccessfulResolution(self):
    """Test successful resolution of alias to IP."""
    services = [
        mdns.Service('d1.local', '1.1.1.1', 0, 'd1.a.local', {'alias': 'd1'}),
        mdns.Service('d2.local', '2.2.2.2', 0, 'd2.a.local', {'alias': 'd2'}),
        mdns.Service('d3.local', '3.3.3.3', 0, 'd3.a.local', {'alias': 'd3'})]
    self._MockNetworkResponse(services)
    ip = remote_access.GetUSBDeviceIP('d2')
    self.assertEqual(ip, '2.2.2.2')

  def testDuplicateAlias(self):
    """Test resolution of alias to IP when duplicate aliases exist."""
    services = [
        mdns.Service('d1.local', '1.1.1.1', 0, 'd1.a.local', {'alias': 'd1'}),
        mdns.Service('d2.local', '2.2.2.2', 0, 'd2.a.local', {'alias': 'd2'}),
        mdns.Service('d2.local', '3.3.3.3', 0, 'd2.a.local', {'alias': 'd2'})]
    self._MockNetworkResponse(services)
    ip = remote_access.GetUSBDeviceIP('d2')
    # Make sure the IP belongs to the first response that matches the alias.
    self.assertEqual(ip, '2.2.2.2')

  def testFailedResolution(self):
    """Test failed resolution of alias to IP."""
    services = [
        mdns.Service('d1.local', '1.1.1.1', 0, 'd1.a.local', {'alias': 'd1'}),
        mdns.Service('d2.local', '2.2.2.2', 0, 'd2.a.local', {'alias': 'd2'})]
    self._MockNetworkResponse(services)
    ip = remote_access.GetUSBDeviceIP('d3')
    self.assertEqual(ip, None)


class TestChromiumOSDeviceHostnameResolution(USBDeviceTestCase):
  """Tests hostname resolution in ChromiumOSDevice."""

  def testHostnameAsNetworkName(self):
    """Test resolving a valid network name."""
    self.PatchObject(socket, 'getaddrinfo')
    hostname = 'good-hostname'
    device = remote_access.ChromiumOSDevice(hostname, connect=False)
    self.assertEqual(device.hostname, hostname)

  def testHostnameAsAlias(self):
    """Test resolving when hostname is used as an alias."""
    hostname = 'good-alias'
    ip = '1.1.1.1'
    self.PatchObject(socket, 'getaddrinfo', side_effect=socket.gaierror)
    self.PatchObject(remote_access, 'GetUSBDeviceIP', return_value=ip)
    device = remote_access.ChromiumOSDevice(hostname, connect=False)
    self.assertEqual(device.hostname, ip)
    self.assertEqual(device._alias, hostname)
    self.assertEqual(remote_access.CONNECTION_TYPE_USB, device.connection_type)

  def testInvalidHostname(self):
    """Test resolving a bad network name and bad alias."""
    hostname = 'bad'
    self.PatchObject(socket, 'getaddrinfo', side_effect=socket.gaierror)
    self.PatchObject(remote_access, 'GetUSBDeviceIP', return_value=None)
    device = remote_access.ChromiumOSDevice(hostname, connect=False)
    # Hostname should be left alone if it's not resolvable.
    self.assertEqual(device.hostname, hostname)

class TestChromiumOSDeviceSetAlias(USBDeviceTestCase):
  """Tests setting alias name in ChromiumOSDevice."""

  def setUp(self):
    """Set up test objects."""
    self.device = remote_access.ChromiumOSDevice('1.1.1.1', connect=False)

  def testAliasTooLong(self):
    """Tests that error is raised when alias is too long."""
    alias_too_long = 'a' * (remote_access.BRILLO_DEVICE_PROPERTY_MAX_LEN + 1)
    with self.assertRaises(remote_access.InvalidDevicePropertyError):
      self.device.SetAlias(alias_too_long)

  def testAliasWithBadChars(self):
    """Tests that error is raised when alias has bad chars."""
    alias_bad_chars = 'bad*'
    with self.assertRaises(remote_access.InvalidDevicePropertyError):
      self.device.SetAlias(alias_bad_chars)

  def testGoodAlias(self):
    """Tests that command runs when good alias is provided."""
    good_alias = 'good'
    run_mock = self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
    self.device.SetAlias(good_alias)
    self.assertTrue(run_mock.called)
