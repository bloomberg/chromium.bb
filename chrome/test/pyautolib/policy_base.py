#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for tests that need to update the policies enforced by Chrome.

Subclasses can call SetPolicies with a dictionary of policies to install.
SetPolicies can also be used to set the device policies on ChromeOS.

The current implementation depends on the platform. The implementations might
change in the future, but tests relying on SetPolicies will keep working.
"""

# On ChromeOS this relies on the device_management.py part of the TestServer,
# and forces the policies by triggering a new policy fetch and refreshing the
# cloud policy providers. This requires preparing the system for policy
# fetching, which currently means the DMTokens have to be in place. Without the
# DMTokens, the cloud policy controller won't be able to proceed, because the
# Gaia tokens for the DMService aren't available during tests.
# In the future this setup might not be necessary anymore, and the policy
# might also be pushed through the session_manager.
#
# On other platforms it relies on the SetPolicies automation call, which is
# only available on non-official builds. This automation call is meant to be
# eventually removed when a replacement for every platform is available.
# This requires setting up the policies in the registry on Windows, and writing
# the right files on Linux and Mac.

import json
import logging
import os
import subprocess
import tempfile
import urllib
import urllib2

import pyauto
import pyauto_paths
import pyauto_utils


if pyauto.PyUITest.IsChromeOS():
  import sys
  # Find the path to the pyproto and add it to sys.path.
  # Prepend it so that google.protobuf is loaded from here.
  for path in pyauto_paths.GetBuildDirs():
    p = os.path.join(path, 'pyproto')
    if os.path.isdir(p):
      sys.path = [p, os.path.join(p, 'chrome', 'browser', 'policy',
                                  'proto')] + sys.path
      break
  sys.path.append('/usr/local')  # to import autotest libs.
  import device_management_local_pb2 as dml
  import device_management_backend_pb2 as dmb
  from autotest.cros import constants
  from autotest.cros import cros_ui
elif pyauto.PyUITest.IsWin():
  import _winreg as winreg


class PolicyTestBase(pyauto.PyUITest):
  """A base class for tests that need to set up and modify policies.

  Subclasses can use the SetPolicies call to set the policies seen by Chrome.
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
    assert self.IsChromeOS()
    return self._http_server.GetURL('device_management').spec()

  def _WriteUserPolicyToken(self, token):
    """Writes the given token to the user device management cache."""
    assert self.IsChromeOS()
    blob = dml.DeviceCredentials()
    blob.device_token = token
    blob.device_id = '123'
    self._WriteFile('/home/chronos/user/Device Management/Token',
                    blob.SerializeToString())

  def _WriteDevicePolicy(self, fetch_response):
    """Writes the given signed fetch_response to the device policy cache.

    Also writes the owner key, used to verify the signature.
    """
    assert self.IsChromeOS()
    self._WriteFile(constants.SIGNED_POLICY_FILE,
                    fetch_response.SerializeToString())
    self._WriteFile(constants.OWNER_KEY_FILE,
                    fetch_response.new_public_key)

  def _PostToDMServer(self, request_type, body, headers):
    """Posts a request to the TestServer's Device Management interface.

    |request_type| is the value of the 'request' HTTP parameter.
    Returns the response's body.
    """
    assert self.IsChromeOS()
    url = self._GetHttpURLForDeviceManagement()
    url += '?' + urllib.urlencode({
      'deviceid': '123',
      'oauth_token': '456',
      'request': request_type,
      'devicetype': 2,
      'apptype': 'Chrome',
      'agent': 'Chrome',
    })
    return urllib2.urlopen(urllib2.Request(url, body, headers)).read()

  def _PostRegisterRequest(self, type):
    """Sends a device register request to the TestServer, of the given type."""
    assert self.IsChromeOS()
    request = dmb.DeviceManagementRequest()
    register = request.register_request
    register.machine_id = '789'
    register.type = type
    return self._PostToDMServer('register', request.SerializeToString(), {})

  def _RegisterAndGetDMToken(self, device):
    """Registers with the TestServer and returns the DMToken fetched.

    Registers for device policy if device is True. Otherwise registers for
    user policy.
    """
    assert self.IsChromeOS()
    type = device and dmb.DeviceRegisterRequest.DEVICE \
                   or dmb.DeviceRegisterRequest.USER
    rstring = self._PostRegisterRequest(type)
    response = dmb.DeviceManagementResponse()
    response.ParseFromString(rstring)
    return response.register_response.device_management_token

  def _PostPolicyRequest(self, token, type, want_signature=False):
    """Fetches policy from the TestServer, using the given token.

    Policy is fetched for the given type. If want_signature is True, the
    request will ask for a signed response. Returns the response body.
    """
    assert self.IsChromeOS()
    request = dmb.DeviceManagementRequest()
    policy = request.policy_request
    prequest = policy.request.add()
    prequest.policy_type = type
    if want_signature:
      prequest.signature_type = dmb.PolicyFetchRequest.SHA1_RSA
    headers = {
      'Authorization': 'GoogleDMToken token=' + token,
    }
    return self._PostToDMServer('policy', request.SerializeToString(), headers)

  def _FetchPolicy(self, token, device):
    """Fetches policy from the TestServer, using the given token.

    Token must be a valid token retrieved with _RegisterAndGetDMToken. If
    device is True, fetches signed device policy. Otherwise fetches user policy.
    This method also verifies the response, and returns the first policy fetch
    response.
    """
    assert self.IsChromeOS()
    type = device and 'google/chromeos/device' or 'google/chromeos/user'
    rstring = self._PostPolicyRequest(token=token, type=type,
                                      want_signature=device)
    response = dmb.DeviceManagementResponse()
    response.ParseFromString(rstring)
    fetch_response = response.policy_response.response[0]
    assert fetch_response.policy_data
    assert fetch_response.policy_data_signature
    assert fetch_response.new_public_key
    return fetch_response

  def _WriteDevicePolicyWithSessionManagerStopped(self, policy):
    """Writes the device policy blob while the Session Manager is stopped."""
    assert self.IsChromeOS()
    logging.debug('Stopping session manager')
    cros_ui.stop(allow_fail=True)
    logging.debug('Writing device policy cache')
    self._WriteDevicePolicy(policy)

    # Ugly hack: session manager won't spawn chrome if this file exists. That's
    # usually a good thing (to keep the automation channel open), but in this
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

    # cros_ui.start() waits for the login prompt to be visible, so chrome has
    # already started once it returns.
    if restore_magic_file:
      open(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE, 'w').close()
      assert os.path.exists(constants.DISABLE_BROWSER_RESTART_MAGIC_FILE)

  def ExtraChromeFlags(self):
    """Sets up Chrome to use cloud policies on ChromeOS."""
    flags = pyauto.PyUITest.ExtraChromeFlags(self)
    if self.IsChromeOS():
      url = self._GetHttpURLForDeviceManagement()
      flag = '--device-management-url=' + url
      flags += [flag]
    return flags

  def setUp(self):
    """Sets up the platform for policy testing.

    On ChromeOS, part of the set up involves restarting the session_manager and
    logging in with the $default account.
    """
    if self.IsChromeOS():
      # Setup a temporary data dir and a TestServer serving files from there.
      # The TestServer makes its document root relative to the src dir.
      self._temp_data_dir = tempfile.mkdtemp(dir=pyauto_paths.GetSourceDir())
      relative_temp_data_dir = os.path.basename(self._temp_data_dir)
      self._http_server = self.StartHTTPServer(relative_temp_data_dir)

      # Setup empty policies, so that the TestServer can start replying.
      self._SetCloudPolicies()

      device_dmtoken = self._RegisterAndGetDMToken(device=True)
      policy = self._FetchPolicy(token=device_dmtoken, device=True)
      user_dmtoken = self._RegisterAndGetDMToken(device=False)

      # The device policy blob is only picked up by the session manager on
      # startup, and is overwritten on shutdown. So the blob has to be written
      # while the session manager is stopped.
      self.WaitForSessionManagerRestart(
          lambda: self._WriteDevicePolicyWithSessionManagerStopped(policy))
      logging.debug('Session manager restarted with device policy ready')

    pyauto.PyUITest.setUp(self)

    if self.IsChromeOS():
      logging.debug('Logging in')
      # TODO(joaodasilva): figure why $apps doesn't work. An @gmail.com account
      # shouldn't fetch policies, but this works after having the tokens.
      credentials = constants.CREDENTIALS['$default']
      self.Login(credentials[0], credentials[1])
      assert self.GetLoginInfo()['is_logged_in']

      self._WriteUserPolicyToken(user_dmtoken)
      # The browser has to be reloaded to make the user policy token cache
      # reload the file just written. The file can also be written only after
      # the cryptohome is mounted, after login.
      self.RestartBrowser(clear_profile=False)

  def tearDown(self):
    """Cleans up the files created by setUp and policies added in tests."""
    # Clear the policies.
    self.SetPolicies()

    if self.IsChromeOS():
      pyauto.PyUITest.Logout(self)

    pyauto.PyUITest.tearDown(self)

    if self.IsChromeOS():
      self.StopHTTPServer(self._http_server)
      pyauto_utils.RemovePath(self._temp_data_dir)

  def _SetCloudPolicies(self, user_mandatory=None, user_recommended=None,
                        device=None):
    """Exports the policies to the configuration file of the TestServer.

    The TestServer will serve these policies after this function returns.

    Args:
      user_mandatory: user policies of mandatory level
      user_recommended: user policies of recommended level
      device: device policies
    """
    assert self.IsChromeOS()
    policy_dict = {
      'google/chromeos/device': device or {},
      'google/chromeos/user': {
        'mandatory': user_mandatory or {},
        'recommended': user_recommended or {},
      },
      'managed_users': ['*'],
    }
    self._WriteFile(self._GetTestServerPoliciesFilePath(),
                    json.dumps(policy_dict, sort_keys=True, indent=2) + '\n')

  def _SetPoliciesWin(self, user_policy=None):
    """Exports the policies as dictionary in the argument to Window registry.

    Removes the registry key and its subkeys if they exist.

    Args:
      user_policy: A dictionary representing the user policies. Clear the
        registry if None.

    Raises:
      TypeError: If an unexpected value is found in the policy dictionary.
    """

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

  def _SetPoliciesLinux(self, user_policy=None):
    """Exports the policies as dictionary in the argument to a JSON file.

    Removes the JSON file if it exists.

    Args:
      user_policy: A dictionary representing the user policies. Remove the
        JSON file if None
    """
    assert self.IsLinux()
    sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                 'policy_linux_util.py')

    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
      policies_location_base = '/etc/opt/chrome'
    else:
      policies_location_base = '/etc/chromium'

    if os.path.isdir(policies_location_base):
      logging.debug('Removing directory %s' % policies_location_base)
      subprocess.call(['suid-python', sudo_cmd_file,
                       'remove_dir', policies_location_base])

    if user_policy is not None:
      self._WriteFile('/tmp/chrome.json',
                      json.dumps(user_policy, sort_keys=True, indent=2) + '\n')

      policies_location = '%s/policies/managed' % policies_location_base
      subprocess.call(['suid-python', sudo_cmd_file,
                       'setup_dir', policies_location])
      # Copy chrome.json file to the managed directory
      subprocess.call(['suid-python', sudo_cmd_file,
                       'copy', '/tmp/chrome.json', policies_location])
      os.remove('/tmp/chrome.json')

  def SetPolicies(self, user_policy=None, device_policy=None):
    """Enforces the policies given in the arguments as dictionaries.

    These policies will have been installed after this call returns.

    Args:
      user_policy: A dictionary representing the user policies.
      device_policy: A dictionary representing the device policies on Chrome OS.

    Raises:
      NotImplementedError if the platform is not supported.
    """
    if self.IsChromeOS():
      self._SetCloudPolicies(user_mandatory=user_policy, device=device_policy)
    else:
      if device_policy is not None:
        raise NotImplementedError('Device policy is only available on ChromeOS')
      if self.IsWin():
        self._SetPoliciesWin(user_policy=user_policy)
      elif self.IsLinux():
        self._SetPoliciesLinux(user_policy=user_policy)
      else:
        raise NotImplementedError('Not available on this platform.')

    self.RefreshPolicies()

