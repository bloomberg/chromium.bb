# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for tests that need to update the policies enforced by Chrome.

Subclasses can call SetUserPolicy (ChromeOS, Linux, Windows) and
SetDevicePolicy (ChromeOS only) with a dictionary of the policies to install.

The current implementation depends on the platform. The implementations might
change in the future, but tests relying on the above calls will keep working.
"""

# On ChromeOS, a mock DMServer is started and enterprise enrollment faked
# against it. The mock DMServer then serves user and device policy to Chrome.
#
# For this setup to work, the DNS, GAIA and TPM (if present) are mocked as well:
#
# * The mock DNS resolves all addresses to 127.0.0.1. This allows the mock GAIA
#   to handle all login attempts. It also eliminates the impact of flaky network
#   connections on tests. Beware though that no cloud services can be accessed
#   due to this DNS redirect.
#
# * The mock GAIA permits login with arbitrary credentials and accepts any OAuth
#   tokens sent to it for verification as valid.
#
# * When running on a real device, its TPM is disabled. If the TPM were enabled,
#   enrollment could not be undone without a reboot. Disabling the TPM makes
#   cryptohomed behave as if no TPM was present, allowing enrollment to be
#   undone by removing the install attributes.
#
# To disable the TPM, 0 must be written to /sys/class/misc/tpm0/device/enabled.
# Since this file is not writeable, a tpmfs is mounted that shadows the file
# with a writeable copy.

import json
import logging
import os
import subprocess

import pyauto

if pyauto.PyUITest.IsChromeOS():
  import sys
  import warnings

  import pyauto_paths

  # Ignore deprecation warnings, they make our output more cluttered.
  warnings.filterwarnings('ignore', category=DeprecationWarning)

  # Find the path to the pyproto and add it to sys.path.
  # Prepend it so that google.protobuf is loaded from here.
  for path in pyauto_paths.GetBuildDirs():
    p = os.path.join(path, 'pyproto')
    if os.path.isdir(p):
      sys.path = [p, os.path.join(p, 'chrome', 'browser', 'policy',
                                  'proto')] + sys.path
      break
  sys.path.append('/usr/local')  # to import autotest libs.

  import dbus
  import device_management_backend_pb2 as dm
  import pyauto_utils
  import string
  import tempfile
  import urllib
  import urllib2
  import uuid
  from autotest.cros import auth_server
  from autotest.cros import constants
  from autotest.cros import cros_ui
  from autotest.cros import dns_server
elif pyauto.PyUITest.IsWin():
  import _winreg as winreg
elif pyauto.PyUITest.IsMac():
  import getpass
  import plistlib

# ASN.1 object identifier for PKCS#1/RSA.
PKCS1_RSA_OID = '\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01'

TPM_SYSFS_PATH = '/sys/class/misc/tpm0'
TPM_SYSFS_ENABLED_FILE = os.path.join(TPM_SYSFS_PATH, 'device/enabled')


class PolicyTestBase(pyauto.PyUITest):
  """A base class for tests that need to set up and modify policies.

  Subclasses can use the methods SetUserPolicy (ChromeOS, Linux, Windows) and
  SetDevicePolicy (ChromeOS only) to set the policies seen by Chrome.
  """

  if pyauto.PyUITest.IsChromeOS():
    # TODO(bartfab): Extend the C++ wrapper that starts the mock DMServer so
    # that an owner can be passed in. Without this, the server will assume that
    # the owner is user@example.com and for consistency, so must we.
    owner = 'user@example.com'
    # Subclasses may override these credentials to fake enrollment into another
    # mode or use different device and machine IDs.
    mode = 'enterprise'
    device_id = string.upper(str(uuid.uuid4()))
    machine_id = 'CROSTEST'

  @staticmethod
  def _Call(command, check=False):
    """Invokes a subprocess and optionally asserts the return value is zero."""
    with open(os.devnull, 'w') as devnull:
      if check:
        return subprocess.check_call(command.split(' '), stdout=devnull)
      else:
        return subprocess.call(command.split(' '), stdout=devnull)

  def _WriteFile(self, path, content):
    """Writes content to path, creating any intermediary directories."""
    if not os.path.exists(os.path.dirname(path)):
      os.makedirs(os.path.dirname(path))
    f = open(path, 'w')
    f.write(content)
    f.close()

  def _GetTestServerPoliciesFilePath(self):
    """Returns the path of the cloud policy configuration file."""
    assert self.IsChromeOS()
    return os.path.join(self._temp_data_dir, 'device_management')

  def _GetHttpURLForDeviceManagement(self):
    """Returns the URL at which the TestServer is serving user policy."""
    assert self.IsChromeOS()
    return self._http_server.GetURL('device_management').spec()

  def _RemoveIfExists(self, filename):
    """Removes a file if it exists."""
    if os.path.exists(filename):
      os.remove(filename)

  def _StartSessionManagerAndChrome(self):
    """Starts the session manager and Chrome.

    Requires that the session manager be stopped already.
    """
    # Ugly hack: session manager will not spawn Chrome if this file exists. That
    # is usually a good thing (to keep the automation channel open), but in this
    # case we really want to restart chrome. PyUITest.setUp() will be called
    # after session manager and chrome have restarted, and will setup the
    # automation channel.
    restore_magic_file = False
    if os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE):
      logging.debug('DISABLE_BROWSER_RESTART_MAGIC_FILE found. '
                    'Removing temporarily for the next restart.')
      restore_magic_file = True
      os.remove(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)
      assert not os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)

    logging.debug('Starting session manager again')
    cros_ui.start()

    # cros_ui.start() waits for the login prompt to be visible, so Chrome has
    # already started once it returns.
    if restore_magic_file:
      open(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE, 'w').close()
      assert os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)

  def _WritePolicyOnChromeOS(self):
    """Updates the mock DMServer's input file with current policy."""
    assert self.IsChromeOS()
    policy_dict = {
      'google/chromeos/device': self._device_policy,
      'google/chromeos/user': {
        'mandatory': self._user_policy,
        'recommended': {},
      },
      'managed_users': ['*'],
    }
    self._WriteFile(self._GetTestServerPoliciesFilePath(),
                    json.dumps(policy_dict, sort_keys=True, indent=2) + '\n')

  @staticmethod
  def _IsCryptohomedReadyOnChromeOS():
    """Checks whether cryptohomed is running and ready to accept DBus calls."""
    assert pyauto.PyUITest.IsChromeOS()
    try:
      bus = dbus.SystemBus()
      proxy = bus.get_object('org.chromium.Cryptohome',
                             '/org/chromium/Cryptohome')
      dbus.Interface(proxy, 'org.chromium.CryptohomeInterface')
    except dbus.DBusException:
      return False
    return True

  def _ClearInstallAttributesOnChromeOS(self):
    """Resets the install attributes."""
    assert self.IsChromeOS()
    self._RemoveIfExists('/home/.shadow/install_attributes.pb')
    self._Call('restart cryptohomed', check=True)
    assert self.WaitUntil(self._IsCryptohomedReadyOnChromeOS)

  def _DMPostRequest(self, request_type, request, headers):
    """Posts a request to the mock DMServer."""
    assert self.IsChromeOS()
    url = self._GetHttpURLForDeviceManagement()
    url += '?' + urllib.urlencode({
      'deviceid': self.device_id,
      'oauth_token': 'dummy_oauth_token_that_is_not_checked_anyway',
      'request': request_type,
      'devicetype': 2,
      'apptype': 'Chrome',
      'agent': 'Chrome',
    })
    response = dm.DeviceManagementResponse()
    response.ParseFromString(urllib2.urlopen(urllib2.Request(
        url, request.SerializeToString(), headers)).read())
    return response

  def _DMRegisterDevice(self):
    """Registers with the mock DMServer and returns the DMToken."""
    assert self.IsChromeOS()
    dm_request = dm.DeviceManagementRequest()
    request = dm_request.register_request
    request.type = dm.DeviceRegisterRequest.DEVICE
    request.machine_id = self.machine_id
    dm_response = self._DMPostRequest('register', dm_request, {})
    return dm_response.register_response.device_management_token

  def _DMFetchPolicy(self, dm_token):
    """Fetches device policy from the mock DMServer."""
    assert self.IsChromeOS()
    dm_request = dm.DeviceManagementRequest()
    policy_request = dm_request.policy_request
    request = policy_request.request.add()
    request.policy_type = 'google/chromeos/device'
    request.signature_type = dm.PolicyFetchRequest.SHA1_RSA
    headers = {'Authorization': 'GoogleDMToken token=' + dm_token}
    dm_response = self._DMPostRequest('policy', dm_request, headers)
    response = dm_response.policy_response.response[0]
    assert response.policy_data
    assert response.policy_data_signature
    assert response.new_public_key
    return response

  def ExtraChromeFlags(self):
    """Sets up Chrome to use cloud policies on ChromeOS."""
    flags = pyauto.PyUITest.ExtraChromeFlags(self)
    if self.IsChromeOS():
      while '--skip-oauth-login' in flags:
        flags.remove('--skip-oauth-login')
      url = self._GetHttpURLForDeviceManagement()
      flags.append('--device-management-url=' + url)
      flags.append('--disable-sync')
    return flags

  def _SetUpWithSessionManagerStopped(self):
    """Sets up the test environment after stopping the session manager."""
    assert self.IsChromeOS()
    logging.debug('Stopping session manager')
    cros_ui.stop(allow_fail=True)

    # Start mock GAIA server.
    self._auth_server = auth_server.GoogleAuthServer()
    self._auth_server.run()

    # Disable TPM if present.
    if os.path.exists(TPM_SYSFS_PATH):
      self._Call('mount -t tmpfs -o size=1k tmpfs %s'
          % os.path.realpath(TPM_SYSFS_PATH), check=True)
      self._WriteFile(TPM_SYSFS_ENABLED_FILE, '0')

    # Clear install attributes and restart cryptohomed to pick up the change.
    self._ClearInstallAttributesOnChromeOS()

    # Set install attributes to mock enterprise enrollment.
    bus = dbus.SystemBus()
    proxy = bus.get_object('org.chromium.Cryptohome',
                            '/org/chromium/Cryptohome')
    install_attributes = {
      'enterprise.device_id': self.device_id,
      'enterprise.domain': string.split(self.owner, '@')[-1],
      'enterprise.mode': self.mode,
      'enterprise.owned': 'true',
      'enterprise.user': self.owner
    }
    interface = dbus.Interface(proxy, 'org.chromium.CryptohomeInterface')
    for name, value in install_attributes.iteritems():
      interface.InstallAttributesSet(name, '%s\0' % value)
    interface.InstallAttributesFinalize()

    # Start mock DNS server that redirects all traffic to 127.0.0.1.
    self._dns_server = dns_server.LocalDns()
    self._dns_server.run()

    # Start mock DMServer.
    source_dir = os.path.normpath(pyauto_paths.GetSourceDir())
    self._temp_data_dir = tempfile.mkdtemp(dir=source_dir)
    logging.debug('TestServer input path: %s' % self._temp_data_dir)
    relative_temp_data_dir = os.path.basename(self._temp_data_dir)
    self._http_server = self.StartHTTPServer(relative_temp_data_dir)

    # Initialize the policy served.
    self._device_policy = {}
    self._user_policy = {}
    self._WritePolicyOnChromeOS()

    # Register with mock DMServer and retrieve initial device policy blob.
    dm_token = self._DMRegisterDevice()
    policy = self._DMFetchPolicy(dm_token)

    # Write the initial device policy blob.
    self._WriteFile(constants.OWNER_KEY_FILE, policy.new_public_key)
    self._WriteFile(constants.SIGNED_POLICY_FILE, policy.SerializeToString())

    # Remove any existing vaults.
    self.RemoveAllCryptohomeVaultsOnChromeOS()

    # Restart session manager and Chrome.
    self._StartSessionManagerAndChrome()

  def _tearDownWithSessionManagerStopped(self):
    """Resets the test environment after stopping the session manager."""
    assert self.IsChromeOS()
    logging.debug('Stopping session manager')
    cros_ui.stop(allow_fail=True)

    # Stop mock GAIA server.
    self._auth_server.stop()

    # Reenable TPM if present.
    if os.path.exists(TPM_SYSFS_PATH):
      self._Call('umount %s' % os.path.realpath(TPM_SYSFS_PATH))

    # Clear install attributes and restart cryptohomed to pick up the change.
    self._ClearInstallAttributesOnChromeOS()

    # Stop mock DNS server.
    self._dns_server.stop()

    # Stop mock DMServer.
    self.StopHTTPServer(self._http_server)

    # Clear the policy served.
    pyauto_utils.RemovePath(self._temp_data_dir)

    # Remove the device policy blob.
    self._RemoveIfExists(constants.OWNER_KEY_FILE)
    self._RemoveIfExists(constants.SIGNED_POLICY_FILE)

    # Remove any existing vaults.
    self.RemoveAllCryptohomeVaultsOnChromeOS()

    # Restart session manager and Chrome.
    self._StartSessionManagerAndChrome()

  def setUp(self):
    """Sets up the platform for policy testing.

    On ChromeOS, part of the setup involves restarting the session manager to
    inject an initial device policy blob.
    """
    if self.IsChromeOS():
      # Perform the remainder of the setup with the device manager stopped.
      self.WaitForSessionManagerRestart(
          self._SetUpWithSessionManagerStopped)

    pyauto.PyUITest.setUp(self)
    self._branding = self.GetBrowserInfo()['properties']['branding']

  def tearDown(self):
    """Cleans up the policies and related files created in tests."""
    if self.IsChromeOS():
      # Perform the cleanup with the device manager stopped.
      self.WaitForSessionManagerRestart(self._tearDownWithSessionManagerStopped)
    else:
      # On other platforms, there is only user policy to clear.
      self.SetUserPolicy(refresh=False)

    pyauto.PyUITest.tearDown(self)

  def LoginWithTestAccount(self, account='prod_enterprise_test_user'):
    """Convenience method for logging in with one of the test accounts."""
    assert self.IsChromeOS()
    credentials = self.GetPrivateInfo()[account]
    self.Login(credentials['username'], credentials['password'])
    assert self.GetLoginInfo()['is_logged_in']

  def _GetCurrentLoginScreenId(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               String(cr.ui.Oobe.getInstance().currentScreen.id));
        """)

  def _WaitForLoginScreenId(self, id):
    self.assertTrue(
        self.WaitUntil(function=self._GetCurrentLoginScreenId,
                       expect_retval=id),
                       msg='Expected login screen "%s" to be visible.' % id)

  def _CheckLoginFormLoading(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               cr.ui.Oobe.getInstance().currentScreen.loading);
        """)

  def _WaitForLoginFormReload(self):
    self.assertEqual('gaia-signin',
                     self._GetCurrentLoginScreenId(),
                     msg='Expected the login form to be visible.')
    self.assertTrue(
        self.WaitUntil(function=self._CheckLoginFormLoading),
                       msg='Expected the login form to start reloading.')
    self.assertTrue(
        self.WaitUntil(function=self._CheckLoginFormLoading,
                       expect_retval=False),
                       msg='Expected the login form to finish reloading.')

  def _SetUserPolicyChromeOS(self, user_policy=None):
    """Writes the given user policy to the mock DMServer's input file."""
    self._user_policy = user_policy or {}
    self._WritePolicyOnChromeOS()

  def _SetUserPolicyWin(self, user_policy=None):
    """Writes the given user policy to the Windows registry."""
    def SetValueEx(key, sub_key, value):
      if isinstance(value, int):
        winreg.SetValueEx(key, sub_key, 0, winreg.REG_DWORD, int(value))
      elif isinstance(value, basestring):
        winreg.SetValueEx(key, sub_key, 0, winreg.REG_SZ, value.encode('ascii'))
      elif isinstance(value, list):
        k = winreg.CreateKey(key, sub_key)
        for index, v in list(enumerate(value)):
          SetValueEx(k, str(index + 1), v)
        winreg.CloseKey(k)
      else:
        raise TypeError('Unsupported data type: "%s"' % value)

    assert self.IsWin()
    if self._branding == 'Google Chrome':
      reg_base = r'SOFTWARE\Policies\Google\Chrome'
    else:
      reg_base = r'SOFTWARE\Policies\Chromium'

    if subprocess.call(
        r'reg query HKEY_LOCAL_MACHINE\%s' % reg_base) == 0:
      logging.debug(r'Removing %s' % reg_base)
      subprocess.call(r'reg delete HKLM\%s /f' % reg_base)

    if user_policy is not None:
      root_key = winreg.CreateKey(winreg.HKEY_LOCAL_MACHINE, reg_base)
      for k, v in user_policy.iteritems():
        SetValueEx(root_key, k, v)
      winreg.CloseKey(root_key)

  def _SetUserPolicyLinux(self, user_policy=None):
    """Writes the given user policy to the JSON policy file read by Chrome."""
    assert self.IsLinux()
    sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                 'policy_posix_util.py')

    if self._branding == 'Google Chrome':
      policies_location_base = '/etc/opt/chrome'
    else:
      policies_location_base = '/etc/chromium'

    if os.path.exists(policies_location_base):
      logging.debug('Removing directory %s' % policies_location_base)
      subprocess.call(['suid-python', sudo_cmd_file,
                       'remove_dir', policies_location_base])

    if user_policy is not None:
      self._WriteFile('/tmp/chrome.json',
                      json.dumps(user_policy, sort_keys=True, indent=2) + '\n')

      policies_location = '%s/policies/managed' % policies_location_base
      subprocess.call(['suid-python', sudo_cmd_file,
                       'setup_dir', policies_location])
      subprocess.call(['suid-python', sudo_cmd_file,
                       'perm_dir', policies_location])
      # Copy chrome.json file to the managed directory
      subprocess.call(['suid-python', sudo_cmd_file,
                       'copy', '/tmp/chrome.json', policies_location])
      os.remove('/tmp/chrome.json')

  def _SetUserPolicyMac(self, user_policy=None):
    """Writes the given user policy to the plist policy file read by Chrome."""
    assert self.IsMac()
    sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                 'policy_posix_util.py')

    if self._branding == 'Google Chrome':
      policies_file_base = 'com.google.Chrome.plist'
    else:
      policies_file_base = 'org.chromium.Chromium.plist'

    policies_location = os.path.join('/Library', 'Managed Preferences',
                                     getpass.getuser())

    if os.path.exists(policies_location):
      logging.debug('Removing directory %s' % policies_location)
      subprocess.call(['suid-python', sudo_cmd_file,
                       'remove_dir', policies_location])

    if user_policy is not None:
      policies_tmp_file = os.path.join('/tmp', policies_file_base)
      plistlib.writePlist(user_policy, policies_tmp_file)
      subprocess.call(['suid-python', sudo_cmd_file,
                       'setup_dir', policies_location])
      # Copy policy file to the managed directory
      subprocess.call(['suid-python', sudo_cmd_file,
                       'copy', policies_tmp_file, policies_location])
      os.remove(policies_tmp_file)

  def SetUserPolicy(self, user_policy=None, refresh=True):
    """Sets the user policy provided as a dict.

    Args:
      user_policy: The user policy to set. None clears it.
      refresh: If True, Chrome will refresh and apply the new policy.
               Requires Chrome to be alive for it.
    """
    if self.IsChromeOS():
      self._SetUserPolicyChromeOS(user_policy=user_policy)
    elif self.IsWin():
      self._SetUserPolicyWin(user_policy=user_policy)
    elif self.IsLinux():
      self._SetUserPolicyLinux(user_policy=user_policy)
    elif self.IsMac():
      self._SetUserPolicyMac(user_policy=user_policy)
    else:
      raise NotImplementedError('Not available on this platform.')

    if refresh:
      self.RefreshPolicies()

  def SetDevicePolicy(self, device_policy=None, refresh=True):
    """Sets the device policy provided as a dict.

    Args:
      device_policy: The device policy to set. None clears it.
      refresh: If True, Chrome will refresh and apply the new policy.
               Requires Chrome to be alive for it.
    """
    assert self.IsChromeOS()
    self._device_policy = device_policy or {}
    self._WritePolicyOnChromeOS()
    if refresh:
      self.RefreshPolicies()
