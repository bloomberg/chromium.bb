# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import BaseHTTPServer
import os
import threading


class _FileRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Sends back file resources relative to the server's |root_dir|."""

  def do_GET(self):
    if self.path.endswith('favicon.ico'):
      self.send_error(404)
      return
    path = os.path.join(self.server.root_dir, *self.path.split('/'))
    with open(path, 'r') as f:
      data = f.read()
    self.send_response(200)
    self.send_header('Content-Length', len(data))
    self.end_headers()
    self.wfile.write(data)


class WebServer(object):
  """An HTTP or HTTPS server that serves files on its own thread."""

  def __init__(self, root_dir, server_cert_and_key_path=None):
    """Starts the web server on its own thread on an ephemeral port.
    It is an HTTP server if parameter server_cert_and_key_path is not provided.
    Otherwise, it is an HTTPS server.

    After this function returns, it is safe to assume the server is ready
    to receive requests.

    Args:
      root_dir: root path to serve files from. This parameter is required.
      server_cert_and_key_path: path to a PEM file containing the cert and key.
                                if it is None, start the server as an HTTP one.
    """
    self._server = BaseHTTPServer.HTTPServer(
        ('127.0.0.1', 0), _FileRequestHandler)
    self._server.root_dir = root_dir
    if server_cert_and_key_path is not None:
      self._is_https_enabled = True
      self._server.socket = ssl.wrap_socket(
          self._server.socket, certfile=server_cert_and_key_path,
          server_side=True)
    else:
      self._is_https_enabled = False

    self._thread = threading.Thread(target=self._server.serve_forever)
    self._thread.start()

  def GetUrl(self):
    """Returns the base URL of the server."""
    if self._is_https_enabled:
      return 'https://127.0.0.1:%s' % self._server.server_port
    return 'http://127.0.0.1:%s' % self._server.server_port

  def Shutdown(self):
    """Shuts down the server synchronously."""
    self._server.shutdown()
    self._thread.join()
