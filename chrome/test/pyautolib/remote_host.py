#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import os
import pickle
import socket
import sys

import pyauto

class RemoteHost(object):
  """Class used as a host for tests that use the PyAuto RemoteProxy.

  This class fires up a listener which waits for a connection from a RemoteProxy
  and receives method call requests. Run python remote_host.py
  remote_host.RemoteHost.RunHost to start up a PyAuto remote instance that you
  can connect to and automate using pyauto.RemoteProxy.
  """
  def __init__(self, host, *args, **kwargs):
    self.StartSocketServer(host)

  def StartSocketServer(self, host):
    listening_socket = socket.socket()
    listening_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listening_socket.bind(host)
    listening_socket.listen(1)
    print 'Listening for incoming connections on port %d.' % host[1]
    self._socket, address = listening_socket.accept()
    print 'Accepted connection from %s:%d.' % address

    while self.Connected():
      self._HandleRPC()

  def StopSocketServer(self):
    if self._socket:
      try:
        self._socket.shutdown(socket.SHUT_RDWR)
        self._socket.close()
      except socket.error:
        pass
      self._socket = None

  def Connected(self):
    return self._socket

  def CreateTarget(self, target_class):
    """Creates an instance of the specified class to serve as the RPC target.

    RPC calls can be made on the target.
    """
    self.target = target_class()

  def _HandleRPC(self):
    """Receives a method call request over the socket and executes the method.

    This method captures stdout and stderr for the duration of the method call,
    and sends those, the return value, and any thrown exceptions back to the
    RemoteProxy.
    """
    # Receive request.
    request = self._socket.recv(4096)
    if not request:
      self.StopSocketServer()
      return
    request = pickle.loads(request)

    # Redirect output to strings.
    old_stdout = sys.stdout
    old_stderr = sys.stderr
    sys.stdout = stdout = cStringIO.StringIO()
    sys.stderr = stderr = cStringIO.StringIO()

    # Make requested method call.
    result = None
    exception = None
    try:
      if getattr(self, request[0], None):
        result = getattr(self, request[0])(*request[1], **request[2])
      else:
        result = getattr(self.target, request[0])(*request[1], **request[2])
    except BaseException, e:
      exception = (e.__class__.__name__, str(e))

    # Put output back to the way it was before.
    sys.stdout = old_stdout
    sys.stderr = old_stderr

    # Package up and send the result of the method call.
    response = pickle.dumps((result, stdout.getvalue(), stderr.getvalue(),
                             exception))
    if self._socket.send(response) != len(response):
      self.StopSocketServer()


if __name__ == '__main__':
  pyauto_suite = pyauto.PyUITestSuite(sys.argv)
  RemoteHost(('', 7410))
  del pyauto_suite
