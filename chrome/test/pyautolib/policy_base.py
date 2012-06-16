# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for tests that need to update the policies enforced by Chrome.

Subclasses can call SetUserPolicy (ChromeOS, Linux, Windows) and
SetDevicePolicy (ChromeOS only) with a dictionary of the policies to install.

The current implementation depends on the platform. The implementations might
change in the future, but tests relying on the above calls will keep working.
"""

# On ChromeOS, both user and device policies are supported. Chrome is set up to
# fetch user policy from a TestServer. A call to SetUserPolicy triggers an
# immediate policy refresh, allowing the effects of user policy changes on a
# running session to be tested.
#
# Device policy is injected by stopping Chrome and the session manager, writing
# a new device policy blob and starting Chrome and the session manager again.
# This means that setting device policy always logs out the current user. It is
# *not* possible to test the effects on a running session. These limitations
# stem from the fact that Chrome will only fetch device policy from a TestServer
# if the device is enterprise owned. Enterprise ownership, in turn, requires
# ownership of the TPM which can only be undone by reboothing the device (and
# hence is not possible in a client test).

import json
import logging
import os
import subprocess
import tempfile

import asn1der
import pyauto
import pyauto_paths
import pyauto_utils


if pyauto.PyUITest.IsChromeOS():
  import sys
  import warnings
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
  sys.path.append(os.path.join(pyauto_paths.GetThirdPartyDir(), 'tlslite'))

  import chrome_device_policy_pb2 as dp
  import device_management_backend_pb2 as dm
  import tlslite.api
  from autotest.cros import constants
  from autotest.cros import cros_ui
elif pyauto.PyUITest.IsWin():
  import _winreg as winreg
elif pyauto.PyUITest.IsMac():
  import getpass
  import plistlib

# ASN.1 object identifier for PKCS#1/RSA.
PKCS1_RSA_OID = '\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01'


class PolicyTestBase(pyauto.PyUITest):
  """A base class for tests that need to set up and modify policies.

  Subclasses can use the methods SetUserPolicy (ChromeOS, Linux, Windows) and
  SetDevicePolicy (ChromeOS only) to set the policies seen by Chrome.
  """

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

  def _WriteDevicePolicyWithSessionManagerStopped(self):
    """Writes the device policy blob while the session manager is stopped.

    Updates the files holding the device policy blob and the public key need to
    verify its signature.
    """
    assert self.IsChromeOS()
    logging.debug('Stopping session manager')
    cros_ui.stop(allow_fail=True)
    logging.debug('Writing device policy blob')
    self._WriteFile(constants.SIGNED_POLICY_FILE, self._device_policy_blob)
    self._WriteFile(constants.OWNER_KEY_FILE, self._public_key)

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

  def setUp(self):
    """Sets up the platform for policy testing.

    On ChromeOS, part of the setup involves restarting the session manager to
    inject a device policy blob.
    """
    if self.IsChromeOS():
      # Set up a temporary data dir and a TestServer serving files from there.
      # The TestServer makes its document root relative to the src dir.
      source_dir = os.path.normpath(pyauto_paths.GetSourceDir())
      self._temp_data_dir = tempfile.mkdtemp(dir=source_dir)
      relative_temp_data_dir = os.path.basename(self._temp_data_dir)
      self._http_server = self.StartHTTPServer(relative_temp_data_dir)

      # Set up an empty user policy so that the TestServer can start replying.
      self._SetUserPolicyChromeOS()

      # Generate a key pair for signing device policy.
      self._private_key = tlslite.api.generateRSAKey(1024)
      algorithm = asn1der.Sequence(
          [asn1der.Data(asn1der.OBJECT_IDENTIFIER, PKCS1_RSA_OID),
           asn1der.Data(asn1der.NULL, '')])
      rsa_pubkey = asn1der.Sequence([asn1der.Integer(self._private_key.n),
                                     asn1der.Integer(self._private_key.e)])
      self._public_key = asn1der.Sequence(
          [algorithm, asn1der.Bitstring(rsa_pubkey)])

      # Clear device policy. This also invokes pyauto.PyUITest.setUp(self).
      self.SetDevicePolicy()

      # Remove any existing vaults.
      self.RemoveAllCryptohomeVaultsOnChromeOS()
    else:
      pyauto.PyUITest.setUp(self)

  def tearDown(self):
    """Cleans up the policies and related files created in tests."""
    if self.IsChromeOS():
      # On ChromeOS, clearing device policy logs out the current user so that
      # no policy is in force. User policy is then permanently cleared at the
      # end of the method after stopping the TestServer.
      self.SetDevicePolicy()
    else:
      # On other platforms, there is only user policy to clear.
      self.SetUserPolicy()

    pyauto.PyUITest.tearDown(self)

    if self.IsChromeOS():
      self.StopHTTPServer(self._http_server)
      pyauto_utils.RemovePath(self._temp_data_dir)
      self.RemoveAllCryptohomeVaultsOnChromeOS()

  def LoginWithTestAccount(self, account='prod_enterprise_test_user'):
    """Convenience method for logging in with one of the test accounts."""
    assert self.IsChromeOS()
    credentials = self.GetPrivateInfo()[account]
    self.Login(credentials['username'], credentials['password'])
    assert self.GetLoginInfo()['is_logged_in']

  def _SetUserPolicyChromeOS(self, user_policy=None):
    """Writes the given user policy to the TestServer's input file."""
    assert self.IsChromeOS()
    policy_dict = {
      'google/chromeos/device': {},
      'google/chromeos/user': {
        'mandatory': user_policy or {},
        'recommended': {},
      },
      'managed_users': ['*'],
    }
    self._WriteFile(self._GetTestServerPoliciesFilePath(),
                    json.dumps(policy_dict, sort_keys=True, indent=2) + '\n')

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
    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
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

    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
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

    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
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

  def SetUserPolicy(self, user_policy=None):
    """Sets the user policy provided as a dict.

    Passing a value of None clears the user policy."""
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

    self.RefreshPolicies()

  def _SetProtobufMessageField(self, group_message, field, field_value):
    """Sets the given field in a protobuf to the given value."""
    if field.label == field.LABEL_REPEATED:
      assert type(field_value) == list
      entries = group_message.__getattribute__(field.name)
      for list_item in field_value:
        entries.append(list_item)
      return
    elif field.type == field.TYPE_BOOL:
      assert type(field_value) == bool
    elif field.type == field.TYPE_STRING:
      assert type(field_value) == str or type(field_value) == unicode
    elif field.type == field.TYPE_INT64:
      assert type(field_value) == int
    elif (field.type == field.TYPE_MESSAGE and
          field.message_type.name == 'StringList'):
      assert type(field_value) == list
      entries = group_message.__getattribute__(field.name).entries
      for list_item in field_value:
        entries.append(list_item)
      return
    else:
      raise Exception('Unknown field type %s' % field.type)
    group_message.__setattr__(field.name, field_value)

  def _GenerateDevicePolicyBlob(self, device_policy=None, owner=None):
    """Generates a signed device policy blob."""

    # Fill in the device settings protobuf.
    device_policy = device_policy or {}
    owner = owner or constants.CREDENTIALS['$mockowner'][0]
    settings = dp.ChromeDeviceSettingsProto()
    for group in settings.DESCRIPTOR.fields:
      # Create protobuf message for group.
      group_message = eval('dp.' + group.message_type.name + '()')
      # Indicates if at least one field was set in |group_message|.
      got_fields = False
      # Iterate over fields of the message and feed them from the policy dict.
      for field in group_message.DESCRIPTOR.fields:
        field_value = None
        if field.name in device_policy:
          got_fields = True
          field_value = device_policy[field.name]
          self._SetProtobufMessageField(group_message, field, field_value)
      if got_fields:
        settings.__getattribute__(group.name).CopyFrom(group_message)

    # Fill in the policy data protobuf.
    policy_data = dm.PolicyData()
    policy_data.policy_type = 'google/chromeos/device'
    policy_data.policy_value = settings.SerializeToString()
    policy_data.username = owner
    serialized_policy_data = policy_data.SerializeToString()

    # Fill in the device management response protobuf.
    response = dm.DeviceManagementResponse()
    fetch_response = response.policy_response.response.add()
    fetch_response.policy_data = serialized_policy_data
    fetch_response.policy_data_signature = (
        self._private_key.hashAndSign(serialized_policy_data).tostring())

    self._device_policy_blob = fetch_response.SerializeToString()

  def _RefreshDevicePolicy(self):
    """Refreshes the device policy in force on ChromeOS."""
    assert self.IsChromeOS()
    # The device policy blob is only picked up by the session manager on
    # startup, and is overwritten on shutdown. So the blob has to be written
    # while the session manager is stopped.
    self.WaitForSessionManagerRestart(
        self._WriteDevicePolicyWithSessionManagerStopped)
    logging.debug('Session manager restarted with device policy ready')
    pyauto.PyUITest.setUp(self)

  def SetDevicePolicy(self, device_policy=None, owner=None):
    """Sets the device policy provided as a dict and the owner on ChromeOS.

    Passing a value of None as the device policy clears it."""
    if not self.IsChromeOS():
      raise NotImplementedError('Device policy is only available on ChromeOS.')

    self._GenerateDevicePolicyBlob(device_policy, owner)
    self._RefreshDevicePolicy()
