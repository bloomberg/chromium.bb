# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for running tests on a single device."""

import contextlib
import httplib
import logging
import os
import tempfile
import time

from pylib import android_commands
from pylib import constants
from pylib import ports
from pylib.chrome_test_server_spawner import SpawningServer
from pylib.flag_changer import FlagChanger
from pylib.forwarder import Forwarder
from pylib.valgrind_tools import CreateTool
# TODO(frankf): Move this to pylib/utils
import lighttpd_server


# A file on device to store ports of net test server. The format of the file is
# test-spawner-server-port:test-server-port
NET_TEST_SERVER_PORT_INFO_FILE = 'net-test-server-ports'


class BaseTestRunner(object):
  """Base class for running tests on a single device.

  A subclass should implement RunTests() with no parameter, so that calling
  the Run() method will set up tests, run them and tear them down.
  """

  def __init__(self, device, tool, build_type, push_deps):
    """
      Args:
        device: Tests will run on the device of this ID.
        shard_index: Index number of the shard on which the test suite will run.
        build_type: 'Release' or 'Debug'.
        push_deps: If True, push all dependencies to the device.
    """
    self.device = device
    self.adb = android_commands.AndroidCommands(device=device)
    self.tool = CreateTool(tool, self.adb)
    self._http_server = None
    self._forwarder = None
    self._forwarder_device_port = 8000
    self.forwarder_base_url = ('http://localhost:%d' %
        self._forwarder_device_port)
    self.flags = FlagChanger(self.adb)
    self.flags.AddFlags(['--disable-fre'])
    self._spawning_server = None
    self._spawner_forwarder = None
    # We will allocate port for test server spawner when calling method
    # LaunchChromeTestServerSpawner and allocate port for test server when
    # starting it in TestServerThread.
    self.test_server_spawner_port = 0
    self.test_server_port = 0
    self.build_type = build_type
    self._push_deps = push_deps

  def _PushTestServerPortInfoToDevice(self):
    """Pushes the latest port information to device."""
    self.adb.SetFileContents(self.adb.GetExternalStorage() + '/' +
                             NET_TEST_SERVER_PORT_INFO_FILE,
                             '%d:%d' % (self.test_server_spawner_port,
                                        self.test_server_port))

  def RunTest(self, test):
    """Runs a test. Needs to be overridden.

    Args:
      test: A test to run.

    Returns:
      Tuple containing:
        (base_test_result.TestRunResults, tests to rerun or None)
    """
    raise NotImplementedError

  def InstallTestPackage(self):
    """Installs the test package once before all tests are run."""
    pass

  def PushDataDeps(self):
    """Push all data deps to device once before all tests are run."""
    pass

  def SetUp(self):
    """Run once before all tests are run."""
    Forwarder.KillDevice(self.adb, self.tool)
    self.InstallTestPackage()
    push_size_before = self.adb.GetPushSizeInfo()
    if self._push_deps:
      logging.warning('Pushing data files to device.')
      self.PushDataDeps()
      push_size_after = self.adb.GetPushSizeInfo()
      logging.warning(
          'Total data: %0.3fMB' %
          ((push_size_after[0] - push_size_before[0]) / float(2 ** 20)))
      logging.warning(
          'Total data transferred: %0.3fMB' %
          ((push_size_after[1] - push_size_before[1]) / float(2 ** 20)))
    else:
      logging.warning('Skipping pushing data to device.')

  def TearDown(self):
    """Run once after all tests are run."""
    self.ShutdownHelperToolsForTestSuite()

  def CopyTestData(self, test_data_paths, dest_dir):
    """Copies |test_data_paths| list of files/directories to |dest_dir|.

    Args:
      test_data_paths: A list of files or directories relative to |dest_dir|
          which should be copied to the device. The paths must exist in
          |DIR_SOURCE_ROOT|.
      dest_dir: Absolute path to copy to on the device.
    """
    for p in test_data_paths:
      self.adb.PushIfNeeded(
          os.path.join(constants.DIR_SOURCE_ROOT, p),
          os.path.join(dest_dir, p))

  def LaunchTestHttpServer(self, document_root, port=None,
                           extra_config_contents=None):
    """Launches an HTTP server to serve HTTP tests.

    Args:
      document_root: Document root of the HTTP server.
      port: port on which we want to the http server bind.
      extra_config_contents: Extra config contents for the HTTP server.
    """
    self._http_server = lighttpd_server.LighttpdServer(
        document_root, port=port, extra_config_contents=extra_config_contents)
    if self._http_server.StartupHttpServer():
      logging.info('http server started: http://localhost:%s',
                   self._http_server.port)
    else:
      logging.critical('Failed to start http server')
    self.StartForwarderForHttpServer()
    return (self._forwarder_device_port, self._http_server.port)

  def _ForwardPort(self, port_pairs):
    """Creates a forwarder instance if needed and forward a port."""
    if not self._forwarder:
      self._forwarder = Forwarder(self.adb, self.build_type)
    self._forwarder.Run(port_pairs, self.tool, '127.0.0.1')

  def StartForwarder(self, port_pairs):
    """Starts TCP traffic forwarding for the given |port_pairs|.

    Args:
      host_port_pairs: A list of (device_port, local_port) tuples to forward.
    """
    self._ForwardPort(port_pairs)

  def StartForwarderForHttpServer(self):
    """Starts a forwarder for the HTTP server.

    The forwarder forwards HTTP requests and responses between host and device.
    """
    self._ForwardPort([(self._forwarder_device_port, self._http_server.port)])

  def RestartHttpServerForwarderIfNecessary(self):
    """Restarts the forwarder if it's not open."""
    # Checks to see if the http server port is being used.  If not forwards the
    # request.
    # TODO(dtrainor): This is not always reliable because sometimes the port
    # will be left open even after the forwarder has been killed.
    if not ports.IsDevicePortUsed(self.adb,
        self._forwarder_device_port):
      self.StartForwarderForHttpServer()

  def ShutdownHelperToolsForTestSuite(self):
    """Shuts down the server and the forwarder."""
    # Forwarders should be killed before the actual servers they're forwarding
    # to as they are clients potentially with open connections and to allow for
    # proper hand-shake/shutdown.
    Forwarder.KillDevice(self.adb, self.tool)
    if self._forwarder:
      self._forwarder.Close()
    if self._http_server:
      self._http_server.ShutdownHttpServer()
    if self._spawning_server:
      self._spawning_server.Stop()
    self.flags.Restore()

  def CleanupSpawningServerState(self):
    """Tells the spawning server to clean up any state.

    If the spawning server is reused for multiple tests, this should be called
    after each test to prevent tests affecting each other.
    """
    if self._spawning_server:
      self._spawning_server.CleanupState()

  def LaunchChromeTestServerSpawner(self):
    """Launches test server spawner."""
    server_ready = False
    error_msgs = []
    # TODO(pliard): deflake this function. The for loop should be removed as
    # well as IsHttpServerConnectable(). spawning_server.Start() should also
    # block until the server is ready.
    # Try 3 times to launch test spawner server.
    for i in xrange(0, 3):
      self.test_server_spawner_port = ports.AllocateTestServerPort()
      self._ForwardPort(
          [(self.test_server_spawner_port, self.test_server_spawner_port)])
      self._spawning_server = SpawningServer(self.test_server_spawner_port,
                                             self.adb,
                                             self.tool,
                                             self._forwarder,
                                             self.build_type)
      self._spawning_server.Start()
      server_ready, error_msg = ports.IsHttpServerConnectable(
          '127.0.0.1', self.test_server_spawner_port, path='/ping',
          expected_read='ready')
      if server_ready:
        break
      else:
        error_msgs.append(error_msg)
      self._spawning_server.Stop()
      # Wait for 2 seconds then restart.
      time.sleep(2)
    if not server_ready:
      logging.error(';'.join(error_msgs))
      raise Exception('Can not start the test spawner server.')
    self._PushTestServerPortInfoToDevice()
