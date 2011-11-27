#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This functional test spawns a web server, and runs chrome to point
at that web server.

The content served contains prefetch requests, and the tests assert that the
webserver logs reflect that.

Run like any functional test:
$ python chrome/test/functional/prefetch.py
in a repo with a built pyautolib

The import of multiprocessing implies python 2.6 is required
"""

import os
import time
import multiprocessing
import Queue
import string
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

import pyauto_functional  # Must be imported before pyauto
import pyauto

# this class handles IPC retrieving server "logs" from our integral
# server.  Each test should clear() the log, and then run asserts on
# the retrieval list.

# at startup, the server puts an int in the queue which is its port,
# we store that for subsequent tests

class ServerLog:
  def clear(self):
    self.log = {}

  def __init__(self,queue):
    self.clear()
    self.port = None
    self.queue = queue

  def _readQueue(self):
    try:
      while True:
        queueval = self.queue.get(False)
        if isinstance(queueval,int):
          self.port = queueval
        else:
         self.log[queueval] = True
    except Queue.Empty:
      return

  def getPort(self):
    if not self.port:
      self._readQueue()
    return self.port

  def isRetrieved(self,path):
    self._readQueue()
    try:
      return self.log[path]
    except KeyError:
      return None

#
# The next few classes run a simple web server that returns log information
# via a multiprocessing.Queue.
#
class AbstractPrefetchServerHandler(BaseHTTPRequestHandler):
  content = {
      "prefetch-origin.html":
        (200, """<html><head>
<link rel="prefetch" href="static-prefetch-target.html">
<script type="text/javascript">
function changeParagraph()
{
  var newPara = document.createElement("p");
  newPara.innerHTML =
        "<link rel=\\"prefetch\\" href=\\"dynamic-prefetch-target.html\\">" +
       "<p>This paragraph contains a dynamic link prefetch.  " +
       "The target of this prefetch is " +
       "<a href=\\"dynamic-prefetch-target.html\\">this document.</a>";
  var para = document.getElementById("p1");
  document.body.insertBefore(newPara,para);
}
</script>
</head>
<body onload="changeParagraph()">
<p id="p1">This is a document that contains a link prefetch.  The target of
that prefetch is <a href="static-prefetch-target.html">this document.</a>
</body>"""),
      "static-prefetch-target.html":
        (200, "<html><head></head><body>empty</body>"),
      "dynamic-prefetch-target.html":
        (200, "<html><head></head><body>empty</body>")}

  def do_GET(self):
    self.queue.put(self.path[1:])
    try:
      response_code, response = self.content[self.path[1:]]
      self.send_response(response_code)
      self.end_headers()
      self.wfile.write(response)
    except KeyError:
      self.send_response(404)
      self.end_headers()

def run_web_server(queue_arg):
  class PrefetchServerHandler(AbstractPrefetchServerHandler):
    queue = queue_arg
  server = HTTPServer(('',0), PrefetchServerHandler)
  queue.put(server.server_port)
  server.serve_forever()

#
# Here's the test itself
#
queue = multiprocessing.Queue()
server_log = ServerLog(queue)

class PrefetchTest(pyauto.PyUITest):
  """Testcase for Prefetching"""
  def testBasic(self):
    server_log.clear()
    url = "http://localhost:%d/prefetch-origin.html" % server_log.getPort()
    self.NavigateToURL(url)
    self.assertEqual(True, server_log.isRetrieved("prefetch-origin.html"))
    time.sleep(0.1)  # required since prefetches occur after onload
    self.assertEqual(True, server_log.isRetrieved(
        "static-prefetch-target.html"))
    self.assertEqual(True, server_log.isRetrieved(
        "dynamic-prefetch-target.html"))

if __name__ == '__main__':
  web_server = multiprocessing.Process(target=run_web_server,args=(queue,))
  web_server.daemon = True
  web_server.start()
  pyauto_functional.Main()
