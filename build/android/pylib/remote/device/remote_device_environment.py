# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Environment setup and teardown for remote devices."""

import os
import sys

from pylib import constants
from pylib.base import environment
from pylib.remote.device import remote_device_helper

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'requests', 'src'))
sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'appurify-python', 'src'))
import appurify.api


class RemoteDeviceEnvironment(environment.Environment):
  """An environment for running on remote devices."""

  def __init__(self, args, error_func):
    """Constructor.

    Args:
      args: Command line arguments.
      error_func: error to show when using bad command line arguments.
    """
    super(RemoteDeviceEnvironment, self).__init__()

    if args.api_key_file:
      with open(args.api_key_file) as api_key_file:
        self._api_key = api_key_file.read().strip()
    elif args.api_key:
      self._api_key = args.api_key
    else:
      error_func('Must set api key with --api-key or --api-key-file')

    if args.api_secret_file:
      with open(args.api_secret_file) as api_secret_file:
        self._api_secret = api_secret_file.read().strip()
    elif args.api_secret:
      self._api_secret = args.api_secret
    else:
      error_func('Must set api secret with --api-secret or --api-secret-file')

    if not args.api_protocol:
      error_func('Must set api protocol with --api-protocol. Example: http')
    self._api_protocol = args.api_protocol

    if not args.api_address:
      error_func('Must set api address with --api-address')
    self._api_address = args.api_address

    if not args.api_port:
      error_func('Must set api port with --api-port.')
    self._api_port = args.api_port

    self._access_token = ''
    self._results_path = args.results_path
    self._remote_device = args.remote_device
    self._remote_device_os = args.remote_device_os
    self._runner_package = args.runner_package
    self._runner_type = args.runner_type
    self._device = ''
    if not args.trigger and not args.collect:
      self._trigger = True
      self._collect = True
    else:
      self._trigger = args.trigger
      self._collect = args.collect

  def SetUp(self):
    """Set up the test environment."""
    os.environ['APPURIFY_API_PROTO'] = self._api_protocol
    os.environ['APPURIFY_API_HOST'] = self._api_address
    os.environ['APPURIFY_API_PORT'] = self._api_port
    self._GetAccessToken()
    if self._trigger:
      self._device = self._SelectDevice()

  def TearDown(self):
    """Teardown the test environment."""
    self._RevokeAccessToken()

  def __enter__(self):
    """Set up the test run when used as a context manager."""
    self.SetUp()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Tears down the test run when used as a context manager."""
    self.TearDown()

  def _GetAccessToken(self):
    """Generates access token for remote device service."""
    access_token_results = appurify.api.access_token_generate(
        self._api_key, self._api_secret)
    remote_device_helper.TestHttpResponse(access_token_results,
                                          'Unable to generate access token.')
    self._access_token = access_token_results.json()['response']['access_token']

  def _RevokeAccessToken(self):
    """Destroys access token for remote device service."""
    revoke_token_results = appurify.api.access_token_revoke(self._access_token)
    remote_device_helper.TestHttpResponse(revoke_token_results,
                                          'Unable to revoke access token.')

  def _SelectDevice(self):
    """Select which device to use."""
    dev_list_res = appurify.api.devices_list(self._access_token)
    remote_device_helper.TestHttpResponse(dev_list_res,
                                          'Unable to generate access token.')
    device_list = dev_list_res.json()['response']
    for device in device_list:
      if (device['name'] == self._remote_device
          and device['os_version'] == self._remote_device_os):
        return device['device_type_id']
    raise remote_device_helper.RemoteDeviceError(
        'No device found: %s %s' % (self._remote_device,
        self._remote_device_os))

  @property
  def device(self):
    return self._device

  @property
  def token(self):
    return self._access_token

  @property
  def results_path(self):
    return self._results_path

  @property
  def runner_type(self):
    return self._runner_type

  @property
  def runner_package(self):
    return self._runner_package

  @property
  def trigger(self):
    return self._trigger

  @property
  def collect(self):
    return self._collect
