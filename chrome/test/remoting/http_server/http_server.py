# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple HTTP server used in Chromoting End-to-end tests.

Serves the static host and client pages with associated javascript files.
Stores state about host actions and communicates it to the client.

Built on CherryPy (http://www.cherrypy.org/) and requires the Chromium
version of CherryPy to be installed from
chromium/tools/build/third_party/cherrypy/.

"""

import json
import os
import sys

try:
  import cherrypy
except ImportError:
  print ('This script requires CherryPy v3 or higher to be installed.\n'
         'Please install and try again.')
  sys.exit(1)


def HttpMethodsAllowed(methods=['GET', 'HEAD']):
  method = cherrypy.request.method.upper()
  if method not in methods:
    cherrypy.response.headers['Allow'] = ', '.join(methods)
    raise cherrypy.HTTPError(405)

cherrypy.tools.allow = cherrypy.Tool('on_start_resource', HttpMethodsAllowed)


class KeyTest(object):
  """Handler for keyboard test in Chromoting E2E tests."""

  keytest_succeeded = False
  keytest_text = None

  @cherrypy.expose
  @cherrypy.tools.allow(methods=['POST'])
  def test(self, text):
    """Stores status of host keyboard actions."""
    self.keytest_succeeded = True
    self.keytest_text = text

  def process(self):
    """Build the JSON message that will be conveyed to the client."""
    message = {
        'keypressSucceeded': self.keytest_succeeded,
        'keypressText': self.keytest_text
    }

    # The message is now built so reset state on the server
    if self.keytest_succeeded:
      self.keytest_succeeded = False
      self.keytest_text = None

    return message


class Root(object):
  """Root Handler for the server."""

  # Every test has its own class which should be instantiated here
  keytest = KeyTest()

  # Every test's class should have a process method that the client polling
  # will call when that test is running.
  # The method should be registered here with the test name.
  TEST_DICT = {
      'keytest': keytest.process
  }

  @cherrypy.expose
  @cherrypy.tools.allow()
  def index(self):
    """Index page to test if server is ready."""
    return 'Simple HTTP Server for Chromoting Browser Tests!'

  @cherrypy.expose
  @cherrypy.tools.allow()
  def poll(self, test):
    """Responds to poll request from client page with status of host actions."""
    if test not in self.TEST_DICT:
      cherrypy.response.status = 500
      return

    cherrypy.response.headers['Content-Type'] = 'application/json'
    return json.dumps(self.TEST_DICT[test]())


app_config = {
    '/': {
        'tools.staticdir.on': True,
        'tools.staticdir.dir':
            os.path.abspath(os.path.dirname(__file__))
    }
}
cherrypy.tree.mount(Root(), '/', config=app_config)
cherrypy.config.update({'server.socket_host': '0.0.0.0',
                        'server.threadpool': 1,
                       })
cherrypy.engine.start()
cherrypy.engine.block()
