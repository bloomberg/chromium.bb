# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing methods and classes to interact with a devserver instance.
"""

import logging
import multiprocessing
import os
import re
import sys
import tempfile
import time
import urllib2

from chromite.buildbot import constants
from chromite.lib import cros_build_lib

# Wait up to 15 minutes for the dev server to start. It can take a while to
# start when generating payloads in parallel.
DEV_SERVER_TIMEOUT = 900
CHECK_HEALTH_URL = 'http://127.0.0.1:8080/check_health'
KILL_TIMEOUT = 10


def GetIPAddress(device='eth0'):
  """Returns the IP Address for a given device using ifconfig.

  socket.gethostname() is insufficient for machines where the host files are
  not set up "correctly."  Since some of our builders may have this issue,
  this method gives you a generic way to get the address so you are reachable
  either via a VM or remote machine on the same network.
  """
  result = cros_build_lib.RunCommandCaptureOutput(
      ['/sbin/ifconfig', device], print_cmd=False)
  match = re.search('.*inet addr:(\d+\.\d+\.\d+\.\d+).*', result.output)
  if match:
    return match.group(1)
  cros_build_lib.Warning('Failed to find ip address in %r', result.output)
  return None


def GenerateUpdateId(target, src, key, for_vm):
  """Returns a simple representation id of target and src paths."""
  update_id = target
  if src: update_id = '->'.join([src, update_id])
  if key: update_id = '+'.join([update_id, key])
  if not for_vm: update_id = '+'.join([update_id, 'patched_kernel'])
  return update_id


class DevServerException(Exception):
  """Thrown when the devserver fails to start up correctly."""


class DevServerWrapper(multiprocessing.Process):
  """A Simple wrapper around a dev server instance."""

  def __init__(self, test_root=None):
    super(DevServerWrapper, self).__init__()
    self.test_root = test_root or DevServerWrapper.MkChrootTemp(is_dir=True)
    self._log_filename = os.path.join(self.test_root, 'dev_server.log')
    self._pid_file = DevServerWrapper.MkChrootTemp(is_dir=False)
    self._pid = None

  # Chroot helper methods. Since we call a script into the chroot with paths
  # created outside the chroot, translation back and forth is needed. These
  # methods all assume the default chroot dir.

  @staticmethod
  def MkChrootTemp(is_dir):
    """Returns a temp(dir if is_dir, file otherwise) in the chroot."""
    chroot_tmp = os.path.join(constants.SOURCE_ROOT,
                              constants.DEFAULT_CHROOT_DIR, 'tmp')
    if is_dir:
      return tempfile.mkdtemp(prefix='devserver_wrapper', dir=chroot_tmp)
    else:
      return tempfile.NamedTemporaryFile(prefix='devserver_wrapper',
                                         dir=chroot_tmp, delete=False).name

  @staticmethod
  def ToChrootPath(path):
    """Converts the path outside the chroot to one that can be used inside."""
    path = os.path.abspath(path)
    if '/chroot/' not in path:
      raise ValueError('Path %s is not a path that points inside the chroot' %
                       path)

    return '/' + path.partition('/chroot/')[2]

  def _WaitUntilStarted(self):
    """Wait until the devserver has started."""
    current_time = time.time()
    deadline = current_time + DEV_SERVER_TIMEOUT
    while current_time <= deadline:
      try:
        if not self.is_alive():
          raise DevServerException('Devserver crashed while starting')

        urllib2.urlopen(CHECK_HEALTH_URL, timeout=0.05)
        return
      except IOError:
        # urlopen errors will throw a subclass of IOError if we can't connect.
        pass
      finally:
        # Let's not churn needlessly in this loop as the devserver starts up.
        time.sleep(1)

      current_time = time.time()
    else:
      self.terminate()
      raise DevServerException('Devserver did not start')

  def run(self):
    """Kicks off devserver in a separate process and waits for it to finish."""
    cmd = ['start_devserver',
           '--pidfile', DevServerWrapper.ToChrootPath(self._pid_file),
           '--logfile', DevServerWrapper.ToChrootPath(self._log_filename)]
    return_obj = cros_build_lib.SudoRunCommand(
        cmd, enter_chroot=True, debug_level=logging.DEBUG,
        cwd=constants.SOURCE_ROOT, error_code_ok=True,
        redirect_stdout=True, combine_stdout_stderr=True)
    if return_obj.returncode != 0:
      logging.error('Devserver terminated unexpectedly!')
      logging.error(return_obj.output)

  def Start(self):
    """Starts a background devserver and waits for it to start.

    Starts a background devserver and waits for it to start. Will only return
    once devserver has started and running pid has been read.
    """
    self.start()
    self._WaitUntilStarted()
    # Pid file was passed into the chroot.
    with open(self._pid_file, 'r') as f:
      self._pid = f.read()

  def Stop(self):
    """Kills the devserver instance with SIGTERM and SIGKILL if SIGTERM fails"""
    if not self._pid:
      logging.error('No devserver running.')
      return

    logging.debug('Stopping devserver instance with pid %s', self._pid)
    if self.is_alive():
      cros_build_lib.SudoRunCommand(['kill', self._pid],
                                    debug_level=logging.DEBUG)
    else:
      logging.error('Devserver not running!')
      return

    self.join(KILL_TIMEOUT)
    if self.is_alive():
      logging.warning('Devserver is unstoppable. Killing with SIGKILL')
      cros_build_lib.SudoRunCommand(['kill', '-9', self._pid],
                                    debug_level=logging.DEBUG)

  def PrintLog(self):
    """Print devserver output to stdout."""
    print '--- Start output from %s ---' % self._log_filename
    # Open in update mode in case the child process hasn't opened the file yet.
    with open(self._log_filename) as log:
      sys.stdout.writelines(log)

    print '--- End output from %s ---' % self._log_filename

  @classmethod
  def GetDevServerURL(cls, port=None, sub_dir=None):
    """Returns the dev server url for a given port and sub directory."""
    if not port: port = 8080
    url = 'http://%(ip)s:%(port)s' % {'ip': GetIPAddress(), 'port': str(port)}
    if sub_dir:
      url += '/' + sub_dir

    return url

  @classmethod
  def WipePayloadCache(cls):
    """Cleans up devserver cache of payloads."""
    cros_build_lib.Info('Cleaning up previously generated payloads.')
    cmd = ['start_devserver', '--clear_cache', '--exit']
    cros_build_lib.SudoRunCommand(
        cmd, enter_chroot=True, print_cmd=False, combine_stdout_stderr=True,
        redirect_stdout=True, redirect_stderr=True, cwd=constants.SOURCE_ROOT)
