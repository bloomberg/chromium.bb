# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple HTTP proxy server."""

import BaseHTTPServer
import copy
import os
import socket
import SocketServer
import threading
import urllib
import urllib2
from urlparse import urlparse

_HOST = '127.0.0.1'


class _ProxyRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Request handler for the proxy server."""

  # Disables buffering, which causes problems with certain sites.
  rbufsize = 0

  def __init__(self, request, client_addr, server):
    BaseHTTPServer.BaseHTTPRequestHandler.__init__(
        self, request, client_addr, server)

  def _GetHandler(self):
    """GET handler for requests that will be processed by the server."""
    try:
      url = urllib.urlopen(self.path, proxies={'http:' : '127.0.0.1'})
    except IOError, err:
      self.wfile.write(err)
      return
    data = url.read()
    self.wfile.write(data)
    url.close()

  def do_GET(self):
    """Handles GET requests."""
    if self._ShouldHandleRequest():
      self._LogRequest()
      self._GetHandler()
    else:
      self._GenericResponseHandler()

  def do_CONNECT(self):
    """Handles CONNECT requests."""
    self._GenericResponseHandler()

  def do_HEAD(self):
    """Handles HEAD requests."""
    self.do_GET()

  def do_POST(self):
    """Handles POST requests."""
    self.do_GET()

  def do_PUT(self):
    """Handles PUT requests."""
    self.do_GET()

  def _GenericResponseHandler(self):
    """Sends a dummy reponse for HTTP requests not handled by the server."""
    # Handle dropped connections.
    try:
      self.send_response(200)
    except (socket.error, socket.gaierror):
      return
    contents = 'Default response given for path: %s' % self.path
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(contents))
    self.end_headers()
    if (self.command != 'HEAD'):
      self.wfile.write(contents)

  def _ShouldHandleRequest(self):
    """Determines if a request should be processed by the server."""
    if self.server.ShouldHandleAllRequests():
      return True
    (scheme, netloc, path, params, query, flag) = urlparse(self.path, 'http')
    paths = self.server.GetPaths()
    if(any([netloc.find(url) >= 0 for url in paths]) or
       any([self.path.find(url) >= 0 for url in paths])):
      return True
    return False

  def _LogRequest(self):
    """Logs requests handled by the server to a buffer."""
    self.server.AddHandledRequest(self.requestline)

  def log_request(self, *args, **kwargs):
    """Overridden base class method that disables request logging."""
    pass


class ProxyServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
  """Creates a threaded proxy server."""

  def __init__(self, port=0, paths=[], handle_all=True):
    """Initializes proxy server settings.

    Args:
      port: Server port number. If zero, the server will select a free port.
      paths: A list containing urls the server will process. If |handle_all| is
          False, the server will only process urls in this list. URLs should be
          passed as follows: ['http://www.google.com', '...',].
      handle_all: Flag that determines if the server will process all requests.
    """
    BaseHTTPServer.HTTPServer.__init__(
        self, (_HOST, port), _ProxyRequestHandler, True)
    self._stopped = False
    self._serving = False
    self._lock = threading.RLock()
    self._paths = list(paths)
    self._handle_all = handle_all
    self._handled_requests = []
    self.StartServer()

  def GetPort(self):
    """Returns the port number the server is serving on."""
    return self.server_port

  def StartServer(self):
    """Starts the proxy server in a new thread."""
    if self._stopped:
      raise RuntimeError('Cannot restart server.')
    if not self._serving:
      self._serving = True
      thread = WorkerThread(self)
      thread.start()

  def Shutdown(self):
    """Shuts down the server."""
    if not self._serving:
      raise RuntimeError('Server is currently inactive.')
    self._serving = False
    self._stopped = True
    try:
      urllib2.urlopen('http://%s:%s' % (self.server_name, self.server_port))
    except urllib2.URLError:
      pass
    self.server_close()

  def handle_request(self):
    """Handles requests while the |_serving| flag is True."""
    while self._serving:
      BaseHTTPServer.HTTPServer.handle_request(self)

  def ShouldHandleAllRequests(self):
    """Determines if server should handle all requests."""
    return self._handle_all

  def AddHandledRequest(self, request):
    """Appends requests handled by the server to |_handled_requests|."""
    try:
      self._lock.acquire()
      self._handled_requests.append(request)
    finally:
      self._lock.release()

  def GetHandledRequests(self):
    """Returns requests handled by the server."""
    try:
      self._lock.acquire()
      return copy.deepcopy(self._handled_requests)
    finally:
      self._lock.release()

  def GetPaths(self):
    """Returns list of urls that will be handled by the server."""
    return self._paths


class WorkerThread(threading.Thread):
  """Creates a worker thread."""

  def __init__(self, server):
    threading.Thread.__init__(self)
    self._server = server

  def run(self):
    """Overridden base class method."""
    print 'Serving on port: %s' % self._server.server_port
    self._server.daemon_threads = True
    self._server.handle_request()
