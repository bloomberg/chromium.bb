# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing methods and classes to interact with a nebraska instance.
"""

from __future__ import print_function

import os
import multiprocessing

from six.moves import urllib

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access
from chromite.lib import timeout_util


NEBRASKA_SOURCE_FILE = os.path.join(constants.SOURCE_ROOT, 'src', 'platform',
                                    'dev', 'nebraska', 'nebraska.py')

# Error msg in loading shared libraries when running python command.
ERROR_MSG_IN_LOADING_LIB = 'error while loading shared libraries'

class Error(Exception):
  """Base exception class of nebraska errors."""


class NebraskaStartupError(Error):
  """Thrown when the nebraska fails to start up."""


class NebraskaStopError(Error):
  """Thrown when the nebraska fails to stop."""


class RemoteNebraskaWrapper(multiprocessing.Process):
  """A wrapper for nebraska.py on a remote device.

  We assume there is no chroot on the device, thus we do not launch
  nebraska inside chroot.
  """
  NEBRASKA_TIMEOUT = 30
  KILL_TIMEOUT = 10

  # Keep in sync with nebraska.py if not passing these directly to nebraska.
  RUNTIME_ROOT = '/run/nebraska'
  PID_FILE_PATH = os.path.join(RUNTIME_ROOT, 'pid')
  PORT_FILE_PATH = os.path.join(RUNTIME_ROOT, 'port')
  LOG_FILE_PATH = '/tmp/nebraska.log'
  REQUEST_LOG_FILE_PATH = '/tmp/nebraska_request_log.json'

  NEBRASKA_PATH = '/usr/local/bin/nebraska.py'

  def __init__(self, remote_device, nebraska_bin=None,
               update_payloads_address=None, update_metadata_dir=None,
               install_payloads_address=None, install_metadata_dir=None):
    """Initializes the nebraska wrapper.

    Args:
      remote_device: A remote_access.RemoteDevice object.
      nebraska_bin: The path to the nebraska binary.
      update_payloads_address: The root address where the payloads will be
          served.  it can either be a local address (file://) or a remote
          address (http://)
      update_metadata_dir: A directory where json files for payloads required
          for update are located.
      install_payloads_address: Same as update_payloads_address for install
          operations.
      install_metadata_dir: Similar to update_metadata_dir but for install
          payloads.
    """
    super(RemoteNebraskaWrapper, self).__init__()

    self._device = remote_device
    self._hostname = remote_device.hostname

    self._update_payloads_address = update_payloads_address
    self._update_metadata_dir = update_metadata_dir
    self._install_payloads_address = install_payloads_address
    self._install_metadata_dir = install_metadata_dir

    self._nebraska_bin = nebraska_bin or self.NEBRASKA_PATH

    self._port_file = self.PORT_FILE_PATH
    self._pid_file = self.PID_FILE_PATH
    self._log_file = self.LOG_FILE_PATH

    self._port = None
    self._pid = None

  def _RemoteCommand(self, *args, **kwargs):
    """Runs a remote shell command.

    Args:
      *args: See remote_access.RemoteDevice documentation.
      **kwargs: See remote_access.RemoteDevice documentation.
    """
    kwargs.setdefault('debug_level', logging.DEBUG)
    return self._device.RunCommand(*args, **kwargs)

  def _PortFileExists(self):
    """Checks whether the port file exists in the remove device or not."""
    result = self._RemoteCommand(
        ['test', '-f', self._port_file], error_code_ok=True)
    return result.returncode == 0

  def _ReadPortNumber(self):
    """Reads the port number from the port file on the remote device."""
    if not self.is_alive():
      raise NebraskaStartupError('Nebraska is not alive, so no port file yet!')

    try:
      timeout_util.WaitForReturnTrue(self._PortFileExists, period=5,
                                     timeout=self.NEBRASKA_TIMEOUT)
    except timeout_util.TimeoutError:
      self.terminate()
      raise NebraskaStartupError('Timeout (%s) waiting for remote nebraska'
                                 ' port_file' % self.NEBRASKA_TIMEOUT)

    self._port = int(self._RemoteCommand(
        ['cat', self._port_file], capture_output=True).output.strip())

  def IsReady(self):
    """Returns True if nebraska is ready to accept requests."""
    if not self.is_alive():
      raise NebraskaStartupError('Nebraska is not alive, so not ready!')

    url = 'http://%s:%d/%s' % (remote_access.LOCALHOST_IP, self._port,
                               'check_health')
    # Running curl through SSH because the port on the device is not accessible
    # by default.
    result = self._RemoteCommand(
        ['curl', url, '-o', '/dev/null'], error_code_ok=True)
    return result.returncode == 0

  def _WaitUntilStarted(self):
    """Wait until the nebraska has started."""
    if not self._port:
      self._ReadPortNumber()

    try:
      timeout_util.WaitForReturnTrue(self.IsReady,
                                     timeout=self.NEBRASKA_TIMEOUT,
                                     period=5)
    except timeout_util.TimeoutError:
      raise NebraskaStartupError('Nebraska did not start.')

    self._pid = int(self._RemoteCommand(
        ['cat', self._pid_file], capture_output=True).output.strip())
    logging.info('Started nebraska with pid %s', self._pid)

  def run(self):
    """Launches a nebraska process on the device.

    Starts a background nebraska and waits for it to finish.
    """
    logging.info('Starting nebraska on %s', self._hostname)

    if not self._update_metadata_dir:
      raise NebraskaStartupError(
          'Update metadata directory location is not passed.')

    cmd = [
        'python', self._nebraska_bin,
        '--update-metadata', self._update_metadata_dir,
    ]

    if self._update_payloads_address:
      cmd += ['--update-payloads-address', self._update_payloads_address]
    if self._install_metadata_dir:
      cmd += ['--install-metadata', self._install_metadata_dir]
    if self._install_payloads_address:
      cmd += ['--install-payloads-address', self._install_payloads_address]

    try:
      self._RemoteCommand(cmd, redirect_stdout=True, combine_stdout_stderr=True)
    except cros_build_lib.RunCommandError as err:
      msg = 'Remote nebraska failed (to start): %s' % str(err)
      logging.error(msg)
      raise NebraskaStartupError(msg)

  def Start(self):
    """Starts the nebraska process remotely on the remote device."""
    if self.is_alive():
      logging.warn('Nebraska is already running, not running again.')
      return

    self.start()
    self._WaitUntilStarted()

  def Stop(self):
    """Stops the nebraska instance if its running.

    Kills the nebraska instance with SIGTERM (and SIGKILL if SIGTERM fails).
    """
    logging.debug('Stopping nebraska instance with pid %s', self._pid)
    if self.is_alive():
      self._RemoteCommand(['kill', str(self._pid)], error_code_ok=True)
    else:
      logging.debug('Nebraska is not running, stopping nothing!')
      return

    self.join(self.KILL_TIMEOUT)
    if self.is_alive():
      logging.warning('Nebraska is unstoppable. Killing with SIGKILL.')
      try:
        self._RemoteCommand(['kill', '-9', str(self._pid)])
      except cros_build_lib.RunCommandError as e:
        raise NebraskaStopError('Unable to stop Nebraska: %s' % e)

  def GetURL(self, ip=remote_access.LOCALHOST_IP,
             critical_update=False, no_update=False):
    """Returns the URL which the devserver is running on.

    Args:
      ip: The ip of running nebraska if different than localhost.
      critical_update: Whether nebraska has to instruct the update_engine that
          the update is a critical one or not.
      no_update: Whether nebraska has to give a noupdate response even if it
          detected an update.

    Returns:
      An HTTP URL that can be passed to the update_engine_client in --omaha_url
          flag.
    """
    query_dict = {}
    if critical_update:
      query_dict['critical_update'] = True
    if no_update:
      query_dict['no_update'] = True
    query_string = urllib.parse.urlencode(query_dict)

    return ('http://%s:%d/update/%s' %
            (ip, self._port, (('?%s' % query_string) if query_string else '')))

  def PrintLog(self):
    """Print Nebraska log to stdout."""
    if self._RemoteCommand(
        ['test', '-f', self._log_file], error_code_ok=True).returncode != 0:
      logging.error('Nebraska log file %s does not exist on the device.',
                    self._log_file)
      return

    result = self._RemoteCommand(['cat', self._log_file], capture_output=True)
    output = '--- Start output from %s ---\n' % self._log_file
    output += result.output
    output += '--- End output from %s ---' % self._log_file
    return output

  def CollectLogs(self, target_log):
    """Copies the nebraska logs from the device.

    Args:
      target_log: The file to copy the log to from the device.
    """
    try:
      self._device.CopyFromDevice(self._log_file, target_log)
    except (remote_access.RemoteAccessException,
            cros_build_lib.RunCommandError) as err:
      logging.error('Failed to copy nebraska logs from device, ignoring: %s',
                    str(err))

  def CollectRequestLogs(self, target_log):
    """Copies the nebraska logs from the device.

    Args:
      target_log: The file to write the log to.
    """
    if not self.is_alive():
      return

    request_log_url = 'http://%s:%d/requestlog' % (remote_access.LOCALHOST_IP,
                                                   self._port)
    try:
      self._RemoteCommand(
          ['curl', request_log_url, '-o', self.REQUEST_LOG_FILE_PATH])
      self._device.CopyFromDevice(self.REQUEST_LOG_FILE_PATH, target_log)
    except (remote_access.RemoteAccessException,
            cros_build_lib.RunCommandError) as err:
      logging.error('Failed to get requestlog from nebraska. ignoring: %s',
                    str(err))

  def CheckNebraskaCanRun(self):
    """Checks to see if we can start nebraska.

    If the stateful partition is corrupted, Python or other packages needed for
    rootfs update may be missing on |device|.

    This will also use `ldconfig` to update library paths on the target
    device if it looks like that's causing problems, which is necessary
    for base images.

    Raise NebraskaStartupError if nebraska cannot start.
    """

    # Try to capture the output from the command so we can dump it in the case
    # of errors. Note that this will not work if we were requested to redirect
    # logs to a |log_file|.
    cmd_kwargs = {'capture_output': True, 'combine_stdout_stderr': True}
    cmd = ['python', self._nebraska_bin, '--help']
    logging.info('Checking if we can run nebraska on the device...')
    try:
      self._RemoteCommand(cmd, **cmd_kwargs)
    except cros_build_lib.RunCommandError as e:
      logging.warning('Cannot start nebraska.')
      logging.warning(e.result.error)
      if ERROR_MSG_IN_LOADING_LIB in str(e):
        logging.info('Attempting to correct device library paths...')
        try:
          self._RemoteCommand(['ldconfig'], **cmd_kwargs)
          self._RemoteCommand(cmd, **cmd_kwargs)
          logging.info('Library path correction successful.')
          return
        except cros_build_lib.RunCommandError as e2:
          logging.warning('Library path correction failed:')
          logging.warning(e2.result.error)
          raise NebraskaStartupError(e.result.error)

      error_msg = e.result.error.splitlines()[-1]
      raise NebraskaStartupError(error_msg)
