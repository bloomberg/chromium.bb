#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try:
  import json
except ImportError:  # < 2.6
  import simplejson as json

import os
import optparse
import platform
import subprocess
import sys
import time
import unittest
import urllib2
from urlparse import urlparse

if sys.version_info[0] <= 2 and sys.version_info[1] < 6:
  # subprocess.Popen.kill is not available prior to 2.6.
  if platform.system() == 'Windows':
    import win32api
  else:
    import signal


WEBDRIVER_EXE = os.path.abspath(os.path.join('.', 'chromedriver'))
if platform.system() == 'Windows':
  WEBDRIVER_EXE = '%s.exe' % WEBDRIVER_EXE
WEBDRIVER_PORT = 8080
WEBDRIVER_SERVER_URL = None
WEBDRIVER_PROCESS = None


def setUpModule():
  """Starts the webdriver server, if necessary."""
  global WEBDRIVER_SERVER_URL
  global WEBDRIVER_PROCESS
  if not WEBDRIVER_SERVER_URL:
    WEBDRIVER_SERVER_URL = 'http://localhost:%d' % WEBDRIVER_PORT
    WEBDRIVER_PROCESS = subprocess.Popen([WEBDRIVER_EXE,
                                          '--port=%d' % WEBDRIVER_PORT])
    time.sleep(3)


def tearDownModule():
  """Kills the WebDriver server, if it was started for testing."""
  global WEBDRIVER_PROCESS
  if WEBDRIVER_PROCESS:
    if sys.version_info[0] <= 2 and sys.version_info[1] < 6:
      # From http://stackoverflow.com/questions/1064335
      if platform.system() == 'Windows':
        PROCESS_TERMINATE = 1
        handle = win32api.OpenProcess(PROCESS_TERMINATE, False,
                                      WEBDRIVER_PROCESS.pid)
        win32api.TerminateProcess(handle, -1)
        win32api.CloseHandle(handle)
      else:
        os.kill(WEBDRIVER_PROCESS.pid, signal.SIGKILL)
    else:
      WEBDRIVER_PROCESS.kill()


class Request(urllib2.Request):
  """Extends urllib2.Request to support all HTTP request types."""

  def __init__(self, url, method=None, data=None):
    """Initialise a new HTTP request.

    Arguments:
      url: The full URL to send the request to.
      method: The HTTP request method to use; defaults to 'GET'.
      data: The data to send with the request as a string. Defaults to
          None and is ignored if |method| is not 'POST' or 'PUT'.
    """
    if method is None:
      method = data is not None and 'POST' or 'GET'
    elif method not in ('POST', 'PUT'):
      data = None
    self.method = method
    urllib2.Request.__init__(self, url, data=data)

  def get_method(self):
    """Returns the HTTP method used by this request."""
    return self.method


def SendRequest(url, method=None, data=None):
  """Sends a HTTP request to the WebDriver server.

  Return values and exceptions raised are the same as those of
  |urllib2.urlopen|.

  Arguments:
    url: The full URL to send the request to.
    method: The HTTP request method to use; defaults to 'GET'.
    data: The data to send with the request as a string. Defaults to
        None and is ignored if |method| is not 'POST' or 'PUT'.

    Returns:
      A file-like object.
  """
  request = Request(url, method=method, data=data)
  request.add_header('Accept', 'application/json')
  opener = urllib2.build_opener(urllib2.HTTPRedirectHandler())
  return opener.open(request)


class WebDriverSessionlessTest(unittest.TestCase):
  """Tests against the WebDriver REST protocol that do not require creating
  a session with the WebDriver server.
  """

  def testShouldReturn404WhenSentAnUnknownCommandURL(self):
    request_url = '%s/foo' % WEBDRIVER_SERVER_URL
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testShouldReturnHTTP405WhenSendingANonPostToTheSessionURL(self):
    request_url = '%s/session' % WEBDRIVER_SERVER_URL
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 405')
    except urllib2.HTTPError, expected:
      self.assertEquals(405, expected.code)
      self.assertEquals('POST', expected.hdrs['Allow'])

  def testShouldGetA404WhenAttemptingToDeleteAnUnknownSession(self):
    request_url = '%s/session/unknown_session_id' % WEBDRIVER_SERVER_URL
    try:
      SendRequest(request_url, method='DELETE')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)


class SessionTest(unittest.TestCase):
  """Tests requiring an active WebDriver session."""

  def setUp(self):
    self.session_url = None

  def tearDown(self):
    if self.session_url:
      response = SendRequest(self.session_url, method='DELETE')
      try:
        self.assertEquals(200, response.code)
      finally:
        response.close()

  def testShouldBeGivenCapabilitiesWhenStartingASession(self):
    request_url = '%s/session' % WEBDRIVER_SERVER_URL
    response = SendRequest(request_url, method='POST', data='{}')
    try:
      self.assertEquals(200, response.code)
      self.session_url = response.geturl()  # TODO(jleyba): verify this URL?

      data = json.loads(response.read())
      self.assertTrue(isinstance(data, dict))
      self.assertEquals(0, data['status'])

      url_parts = urlparse(self.session_url)[2].split('/')
      self.assertEquals(3, len(url_parts))
      self.assertEquals('', url_parts[0])
      self.assertEquals('session', url_parts[1])
      self.assertEquals(data['sessionId'], url_parts[2])

      capabilities = data['value']
      self.assertTrue(isinstance(capabilities, dict))

      self.assertEquals('chrome', capabilities['browserName'])
      self.assertTrue(capabilities['javascriptEnabled'])

      # Value depends on what version the server is starting.
      self.assertTrue('version' in capabilities)
      self.assertTrue(
          isinstance(capabilities['version'], unicode),
          'Expected a %s, but was %s' % (unicode,
                                         type(capabilities['version'])))

      system = platform.system()
      if system == 'Linux':
        self.assertEquals('linux', capabilities['platform'].lower())
      elif system == 'Windows':
        self.assertEquals('windows', capabilities['platform'].lower())
      elif system == 'Darwin':
        self.assertEquals('mac', capabilities['platform'].lower())
      else:
        # No python on ChromeOS, so we won't have a platform value, but
        # the server will know and return the value accordingly.
        self.assertEquals('chromeos', capabilities['platform'].lower())
    finally:
      response.close()


if __name__ == '__main__':
  parser = optparse.OptionParser('%prog [options]')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None,
                    help=('Specifies the URL of a remote WebDriver server to '
                          'test against. If not specified, a server will be '
                          'started on localhost according to the --exe and '
                          '--port flags'))
  parser.add_option('-e', '--exe', dest='exe', action='store',
                    type='string', default=None,
                    help=('Path to the WebDriver server executable that should '
                          'be started for testing; This flag is ignored if '
                          '--url is provided for a remote server.'))
  parser.add_option('-p', '--port', dest='port', action='store',
                    type='int', default=8080,
                    help=('The port to start the WebDriver server executable '
                          'on; This flag is ignored if --url is provided for a '
                          'remote server.'))

  (options, args) = parser.parse_args()
  # Strip out our flags so unittest.main() correct parses the remaining
  sys.argv = sys.argv[:1]
  sys.argv.extend(args)

  if options.url:
    WEBDRIVER_SERVER_URL = options.url
  else:
    if options.port:
      WEBDRIVER_PORT = options.port
    if options.exe:
      WEBDRIVER_EXE = options.exe
    if not os.path.exists(WEBDRIVER_EXE):
      parser.error('WebDriver server executable not found:\n\t%s\n'
                   'Please specify a valid path with the --exe flag.'
                   % WEBDRIVER_EXE)

  setUpModule()
  try:
    unittest.main()
  finally:
    tearDownModule()
