# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module contains communication methods between cbuildbot instances."""

import Queue
import SocketServer
import os
import socket
import sys
import time

sys.path.append(os.path.join(os.path.dirname(__file__), '../lib'))
from cros_build_lib import Info, Warning, RunCommand

# Communication port for master to slave communication.
_COMM_PORT = 32890
# TCP Buffer Size.
_BUFFER = 4096
# Timeout between checks for new status by either end.
_HEARTBEAT_TIMEOUT = 60 # in sec.
# Max Timeout to wait before assuming failure.
_MAX_TIMEOUT = 30 * 60 # in sec.

# Commands - sent to slave from master.

# Report whether you have completed or failed building.
_COMMAND_CHECK_STATUS = 'check-status'

# Return status - response to commands from slaves (self.explanatory)
_STATUS_COMMAND_REJECTED = 'rejected'
_STATUS_TIMEOUT = 'timeout'
# Public for cbuildbot.
STATUS_BUILD_COMPLETE = 'complete'
STATUS_BUILD_FAILED = 'failure'

# Global queues to communicate with server.
_status_queue = Queue.Queue(1)
_receive_queue = Queue.Queue(1)
_command_queue = Queue.Queue(1)

class _TCPServerWithReuse(SocketServer.TCPServer):
  """TCPServer that allows re-use of socket and timed out sockets."""
  SocketServer.TCPServer.allow_reuse_address = True

  def __init__(self, address, handler, timeout):
    SocketServer.TCPServer.__init__(self, address, handler)
    self.socket.settimeout(timeout)


class _SlaveCommandHandler(SocketServer.BaseRequestHandler):
    """Handles requests from a master pre-flight-queue bot."""

    def _HandleCommand(self, command, args):
      """Handles command and returns status for master."""
      Info('(Slave) - Received command %s with args %s' % (command, args))
      command_to_expect = _command_queue.get()
      # Check status also adds an entry on the status queue.
      if command_to_expect == _COMMAND_CHECK_STATUS:
        slave_status = _status_queue.get()
      # Safety check to make sure the server is in a good state.
      if command_to_expect != command:
        Warning(
            '(Slave) - Rejecting command %s.  Was expecting %s.' % (command,
                command_to_expect))
        return _STATUS_COMMAND_REJECTED
      # Give slave command with optional args.
      _receive_queue.put(args)
      if command == _COMMAND_CHECK_STATUS:
        # Returns status to send.
        return slave_status

    def handle(self):
      """Overriden.  Handles commands sent from master."""
      data = self.request.recv(_BUFFER).strip()
      (command, args) = data.split('\n')
      response = self._HandleCommand(command, args)
      self.request.send(response)


def _GetSlaveNames(configuration):
  """Returns an array of slave hostnames that are important."""
  slaves = []
  for slave_config in configuration.items():
    if (not slave_config[1]['master'] and
        slave_config[1]['important']):
      slaves.append(slave_config[1]['hostname'])
  return slaves


def _SendCommand(hostname, command, args):
  """Returns response from host or _STATUS_TIMEOUT on error."""
  data = '%s\n%s\n' % (command, args)
  Info('(Master) - Sending %s %s to %s' % (command, args, hostname))

  # Create a socket (SOCK_STREAM means a TCP socket).
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

  try:
    # Connect to server and send data
    sock.connect((hostname, _COMM_PORT))
    sock.send(data)

    # Receive data from the server and shut down.
    received = sock.recv(_BUFFER)
  except:
    received = _STATUS_TIMEOUT
  finally:
    sock.close()
  return received


def _CheckSlavesLeftStatus(slaves_to_check):
  """Returns True if remaining slaves have completed.

  Once a slave reports STATUS_BUILD_COMPLETE, removes slave from list.  Returns
  True as long as no slave reports STATUS_BUILD_FAILED.

  Keyword arguments:
  slaves_to_check -- Array of hostnames to check.

  """
  slaves_to_remove = []
  for slave in slaves_to_check:
    status = _SendCommand(slave, _COMMAND_CHECK_STATUS, 'empty')
    if status == STATUS_BUILD_FAILED:
      Warning('(Master) - Slave %s failed' % slave)
      return False
    elif status == STATUS_BUILD_COMPLETE:
      Info('(Master) - Slave %s completed' % slave)
      slaves_to_remove.append(slave)
  for slave in slaves_to_remove:
    slaves_to_check.remove(slave)
  return True


def HaveSlavesCompleted(configuration):
  """Returns True if all other slaves have succeeded.

  Checks other slaves status until either '_MAX_TIMEOUT' has passed,
  at least one slaves reports a failure, or all slaves report success.

  Keyword arguments:
  configuration -- configuration dictionary for slaves.

  """
  not_failed = True
  slaves_to_check = _GetSlaveNames(configuration)
  timeout = 0
  while slaves_to_check and not_failed and timeout < _MAX_TIMEOUT:
    not_failed = _CheckSlavesLeftStatus(slaves_to_check)
    if slaves_to_check and not_failed:
      time.sleep(_HEARTBEAT_TIMEOUT)
      timeout += _HEARTBEAT_TIMEOUT
  return len(slaves_to_check) == 0


def PublishStatus(status):
  """Publishes status and Returns True if master received it.

  This call is blocking until either the master pre-flight-queue bot picks
  up the status, or a '_MAX_TIMEOUT' has passed.

  Keyword arguments:
  status -- should be a string and one of STATUS_BUILD_.*.

  """
  # Clean up queues.
  try:
    _command_queue.get_nowait()
  except Queue.Empty:  pass
  try:
    _status_queue.get_nowait()
  except Queue.Empty:  pass

  _command_queue.put(_COMMAND_CHECK_STATUS)
  _status_queue.put(status)
  server = _TCPServerWithReuse(('localhost', _COMM_PORT),
                                _SlaveCommandHandler, _HEARTBEAT_TIMEOUT)
  timeout = 0
  response = None
  try:
    while not response and timeout < _MAX_TIMEOUT:
      server.handle_request()
      try:
        response = _receive_queue.get_nowait()
      except Queue.Empty:
        Info('(Slave) - Waiting for master to accept %s' % status)
        timeout += _HEARTBEAT_TIMEOUT
        response = None
  except Exception, e:
    Warning('%s' % e)
  server.server_close()
  return response != None
