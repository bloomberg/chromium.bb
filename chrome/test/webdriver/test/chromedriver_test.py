# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Factory that creates ChromeDriver instances."""

import BaseHTTPServer
import os
import ssl
import threading
import unittest

from chromedriver_factory import ChromeDriverFactory
from chromedriver_launcher import ChromeDriverLauncher
import test_paths


class _FileRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Sends back file resources relative to the server's |root_dir|."""

  def do_GET(self):
    if self.path.endswith('favicon.ico'):
      self.send_error(404)
      return
    path = os.path.join(self.server.root_dir, *self.path.split('/'))
    data = open(path, 'r').read()
    self.send_response(200)
    self.send_header('Content-Length', len(data))
    self.end_headers()
    self.wfile.write(data)


class _HttpsServer(object):
  """An HTTPS server that serves files on its own thread."""

  def __init__(self, server_cert_and_key_path, root_dir):
    """Starts the HTTPS server on its own thread on an ephemeral port.

    After this function returns, it is safe to assume the server is ready
    to receive requests.

    Args:
      server_cert_and_key_path: path to a PEM file containing the cert and key.
      root_dir: root path to serve files from.
    """
    self._server = BaseHTTPServer.HTTPServer(
        ('127.0.0.1', 0), _FileRequestHandler)
    self._server.root_dir = root_dir
    self._server.socket = ssl.wrap_socket(
        self._server.socket, certfile=server_cert_and_key_path,
        server_side=True)

    self._thread = threading.Thread(target=self._server.serve_forever)
    self._thread.start()

  def GetUrl(self):
    """Returns the base URL of the server."""
    return 'https://127.0.0.1:%s' % self._server.server_port

  def Shutdown(self):
    """Shuts down the server synchronously."""
    self._server.shutdown()
    self._thread.join()


class ChromeDriverTest(unittest.TestCase):
  """Fixture for tests that need to instantiate ChromeDriver(s)."""

  @staticmethod
  def GlobalSetUp(other_driver=None, other_chrome=None):
    driver_path = other_driver or test_paths.CHROMEDRIVER_EXE
    chrome_path = other_chrome or test_paths.CHROME_EXE
    if driver_path is None or not os.path.exists(driver_path):
      raise RuntimeError('ChromeDriver could not be found')
    if chrome_path is None or not os.path.exists(chrome_path):
      raise RuntimeError('Chrome could not be found')

    ChromeDriverTest._driver_path = driver_path
    ChromeDriverTest._chrome_path = chrome_path
    ChromeDriverTest._server = ChromeDriverLauncher(driver_path).Launch()
    ChromeDriverTest._http_server = ChromeDriverLauncher(
        driver_path, test_paths.TEST_DATA_PATH).Launch()
    ChromeDriverTest._https_server = _HttpsServer(
        test_paths.PEM_CERT_AND_KEY, test_paths.TEST_DATA_PATH)

  @staticmethod
  def GlobalTearDown():
    ChromeDriverTest._server.Kill()
    ChromeDriverTest._http_server.Kill()
    ChromeDriverTest._https_server.Shutdown()

  @staticmethod
  def GetTestDataUrl():
    """Returns the base http url for serving files from the test dir."""
    return ChromeDriverTest._http_server.GetUrl()

  @staticmethod
  def GetHttpsTestDataUrl():
    """Returns the base https url for serving files from the test dir."""
    return ChromeDriverTest._https_server.GetUrl()

  @staticmethod
  def GetDriverPath():
    """Returns the path to the default ChromeDriver binary to use."""
    return ChromeDriverTest._driver_path

  @staticmethod
  def GetChromePath():
    """Returns the path to the default Chrome binary to use."""
    return ChromeDriverTest._chrome_path

  def setUp(self):
    self._factory = ChromeDriverFactory(self._server,
                                        self.GetChromePath())

  def tearDown(self):
    self._factory.QuitAll()

  def GetServer(self):
    """Returns the ChromeDriver server being used."""
    return self._server

  def GetNewDriver(self, capabilities={}):
    """Returns a new RemoteDriver instance."""
    self.assertTrue(self._factory, 'ChromeDriverTest.setUp must be called')
    return self._factory.GetNewDriver(capabilities)
