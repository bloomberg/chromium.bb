# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reverse Port Forwarding class to provide webpagereplay to mobile devices."""

from selenium import webdriver
import os
import sys
# TODO(chris) this path will be subject to change as we put the ispy
#  file system together.
sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)),
                             os.pardir, os.pardir, os.pardir, os.pardir,
                             os.pardir, os.pardir, 'build', 'android'))
from pylib import android_commands
from pylib import forwarder
from pylib import valgrind_tools


class ReversePortForwarder(object):
  """A class that provides reverse port forwarding functionality."""

  def __init__(self, device_http_port, device_https_port,
               host_http_port, host_https_port, device_serial):
    """Makes an instance of ReversePortForwarder.

    Args:
      device_http_port: Http port on the device to forward data from.
      device_https_port: Https port on the device to forward data from.
      host_http_port: Http port on the host to forward data to.
      host_https_port: Http port on the host to forward data to.
      device_serial: The serial id of the device to connect with the host.
    Returns:
      An instance of ReversePortForwarder.
    """
    self._device_http = device_http_port
    self._device_https = device_https_port
    self._host_http = host_http_port
    self._host_https = host_https_port
    self._device_serial = device_serial

  def __enter__(self):
    """Starts the reverse port forwarding in the enter/exit decorator."""
    self.Start()
    return self

  def __exit__(self, t, v, tb):
    """Stops the reverse port forwarding in the enter/exit decorator."""
    self.Stop()

  def Start(self):
    """Sets up reverse port forwarding with a remote webdriver."""
    # Get an adb server running for a given device.
    self._adb = android_commands.AndroidCommands(self._device_serial)
    self._adb.StartAdbServer()
    # Begin forwarding the device_ports to the host_ports.
    self._forwarder = forwarder.Forwarder(self._cmd, 'Release')
    self._forwarder.Run([
        (self._device_http, self._host_http),
        (self._device_https, self._host_https)],
                        valgrind_tools.BaseTool())

  def Stop(self):
    """Cleans up after the start call by closing the forwarder."""
    # shut down the forwarder.
    self._forwarder.Close()

  def GetChromeArgs(self):
    """Makes a list of arguments to enable reverse port forwarding on chrome.

    Returns:
      A list of chrome arguments to make it work with ReversePortForwarder.
    """
    args = ['testing-fixed-http-port=%s' % self._device_http,
            'testing-fixed-https-port=%s' % self._device_https,
            'host-resolver-rules=\'MAP * 127.0.0.1,EXCEPT, localhost\'']
    return args
