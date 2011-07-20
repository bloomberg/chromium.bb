#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import pickle
import socket
import sys

import pyauto


class RemoteHost(pyauto.PyUITest):
  """Class used as a host for tests that use the PyAuto RemoteProxy.

  This class fires up a listener which waits for a connection from a RemoteProxy
  and receives method call requests. Run python remote_host.py
  remote_host.RemoteHost.RunHost to start up a PyAuto remote instance that you
  can connect to and automate using pyauto.RemoteProxy.
  """
  def __init__(self, *args, **kwargs):
    pyauto.PyUITest.__init__(self, *args, **kwargs)
    self.StartSocketServer()

  # We give control of setUp and tearDown to the RemoteProxy, so disable them.
  # The RemoteProxy can call RemoteSetUp() and RemoteTearDown() to do the normal
  # setUp() and tearDown().
  def setUp(self):
    pass

  def tearDown(self):
    pass

  def RemoteSetUp(self):
    pyauto.PyUITest.setUp(self)

  def RemoteTearDown(self):
    pyauto.PyUITest.tearDown(self)

  def StartSocketServer(self, port=7410):
    listening_socket = socket.socket()
    listening_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listening_socket.bind(('', port))
    listening_socket.listen(1)
    self._socket, _ = listening_socket.accept()

  def StopSocketServer(self):
    if self._socket:
      self._socket.shutdown(socket.SHUT_RDWR)
      self._socket.close()
      self._socket = None

  def Connected(self):
    return self._socket

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
      result = getattr(self, request[0])(*request[1], **request[2])
    except BaseException as e:
      exception = (e.__class__.__name__, str(e))

    # Put output back to the way it was before.
    sys.stdout = old_stdout
    sys.stderr = old_stderr

    # Package up and send the result of the method call.
    response = pickle.dumps((result, stdout.getvalue(), stderr.getvalue(),
                             exception))
    if self._socket.send(response) != len(response):
      self.StopSocketServer()

  def RunHost(self):
    while self.Connected():
      self._HandleRPC()


if __name__ == '__main__':
  pyauto.Main()
