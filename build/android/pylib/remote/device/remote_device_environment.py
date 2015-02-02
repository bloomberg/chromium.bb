# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Environment setup and teardown for remote devices."""

import logging
import os
import random
import sys

from pylib import constants
from pylib.base import environment
from pylib.remote.device import appurify_sanitized
from pylib.remote.device import remote_device_helper

class RemoteDeviceEnvironment(environment.Environment):
  """An environment for running on remote devices."""

  _ENV_KEY = 'env'
  _DEVICE_KEY = 'device'

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
    self._verbose_count = args.verbose_count
    self._device_type = args.device_type
    self._timeouts = {
        'queueing': 60 * 10,
        'installing': 60 * 10,
        'in-progress': 60 * 30,
        'unknown': 60 * 5
    }

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
    try:
      self.SetUp()
      return self
    except:
      self.__exit__(*sys.exc_info())
      raise

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Tears down the test run when used as a context manager."""
    self.TearDown()

  def DumpTo(self, persisted_data):
    env_data = {
      self._DEVICE_KEY: self._device,
    }
    persisted_data[self._ENV_KEY] = env_data

  def LoadFrom(self, persisted_data):
    env_data = persisted_data[self._ENV_KEY]
    self._device = env_data[self._DEVICE_KEY]

  def _GetAccessToken(self):
    """Generates access token for remote device service."""
    logging.info('Generating remote service access token')
    with appurify_sanitized.SanitizeLogging(self._verbose_count,
                                            logging.WARNING):
      access_token_results = appurify_sanitized.api.access_token_generate(
          self._api_key, self._api_secret)
    remote_device_helper.TestHttpResponse(access_token_results,
                                          'Unable to generate access token.')
    self._access_token = access_token_results.json()['response']['access_token']

  def _RevokeAccessToken(self):
    """Destroys access token for remote device service."""
    logging.info('Revoking remote service access token')
    with appurify_sanitized.SanitizeLogging(self._verbose_count,
                                            logging.WARNING):
      revoke_token_results = appurify_sanitized.api.access_token_revoke(
          self._access_token)
    remote_device_helper.TestHttpResponse(revoke_token_results,
                                          'Unable to revoke access token.')

  def _SelectDevice(self):
    """Select which device to use."""
    logging.info('Finding device to run tests on.')
    with appurify_sanitized.SanitizeLogging(self._verbose_count,
                                            logging.WARNING):
      dev_list_res = appurify_sanitized.api.devices_list(self._access_token)
    remote_device_helper.TestHttpResponse(dev_list_res,
                                          'Unable to generate access token.')
    device_list = dev_list_res.json()['response']
    random.shuffle(device_list)
    for device in device_list:
      if device['os_name'] != self._device_type:
        continue
      if self._remote_device and device['name'] != self._remote_device:
        continue
      if (self._remote_device_os
          and device['os_version'] != self._remote_device_os):
        continue
      if ((self._remote_device and self._remote_device_os)
          or device['available_devices_count']):
        logging.info('Found device: %s %s',
                     device['name'], device['os_version'])
        return device
    self._NoDeviceFound(device_list)

  def _PrintAvailableDevices(self, device_list):
    def compare_devices(a,b):
      for key in ('os_version', 'name'):
        c = cmp(a[key], b[key])
        if c:
          return c
      return 0

    logging.critical('Available %s Devices:', self._device_type)
    devices = (d for d in device_list if d['os_name'] == self._device_type)
    for d in sorted(devices, compare_devices):
      logging.critical('  %s %s', d['os_version'].ljust(7), d['name'])

  def _NoDeviceFound(self, device_list):
    self._PrintAvailableDevices(device_list)
    raise remote_device_helper.RemoteDeviceError('No device found.')

  @property
  def collect(self):
    return self._collect

  @property
  def device_type_id(self):
    return self._device['device_type_id']

  @property
  def only_output_failures(self):
    # TODO(jbudorick): Remove this once b/18981674 is fixed.
    return True

  @property
  def results_path(self):
    return self._results_path

  @property
  def runner_package(self):
    return self._runner_package

  @property
  def runner_type(self):
    return self._runner_type

  @property
  def timeouts(self):
    return self._timeouts

  @property
  def token(self):
    return self._access_token

  @property
  def trigger(self):
    return self._trigger

  @property
  def verbose_count(self):
    return self._verbose_count

  @property
  def device_type(self):
    return self._device_type
