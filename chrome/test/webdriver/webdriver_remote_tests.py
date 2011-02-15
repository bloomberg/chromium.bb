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
import string
import subprocess
import sys
import time
import types
import unittest
import urllib2
from selenium.common.exceptions import ErrorInResponseException
from selenium.common.exceptions import NoSuchElementException
from selenium.remote.webdriver import WebDriver
from selenium.remote.webdriver.webelement import WebElement
from urlparse import urlparse


if sys.version_info < (2,6):
  # Subprocess.Popen.kill is not available prior to 2.6.
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

if not WEBDRIVER_SERVER_URL:
  WEBDRIVER_SERVER_URL = 'http://localhost:%d' % WEBDRIVER_PORT

class RemoteWebDriverTest(unittest.TestCase):
  SEARCH = "http://www.google.com/webhp?hl=en"
  NEWS = "http://www.google.com/news?hl=en"

  """Verifies that navigation to a specific page is correct by asserting on the
  the reported URL.  The function will not work with pages that redirect."""
  def navigate(self, url):
    self.driver.get(url)
    self.assertURL(url)

  def assertURL(self, url):
    u = self.driver.get_current_url()
    self.assertEqual(u, url)

  """A new instance of chrome driver is started for every test case"""
  def setUp(self):
    global WEBDRIVER_SERVER_URL
    global WEBDRIVER_PROCESS
    WEBDRIVER_PROCESS = subprocess.Popen([WEBDRIVER_EXE,
                                        '--port=%d' % WEBDRIVER_PORT])
    if WEBDRIVER_PROCESS == None:
      print "Chromium executable not found.  The path used was: "
      print WEBDRIVER_EXE
      sys.exit(-1)

    time.sleep(5)
    self.driver = WebDriver.WebDriver(WEBDRIVER_SERVER_URL, "chrome", "ANY")
    self.assertTrue(self.driver)

  def tearDown(self):
    global WEBDRIVER_PROCESS
    self.driver.quit()
    if WEBDRIVER_PROCESS:
      if sys.version_info < (2,6):
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
        WEBDRIVER_PROCESS = None

  """Preforms a string search ignoring case"""
  def assertFind(self, text, search):
    text = string.lower(text)
    search = string.lower(search)
    self.assertNotEqual(-1, string.find(text, search))


class TestFindElement(RemoteWebDriverTest):
  def testFindByName(self):
    self.navigate(SEARCH)
    # Find the Google search button.
    q = self.driver.find_element_by_name("q")
    self.assertTrue(isinstance(q, WebElement))
    # Trying looking for an element not on the page.
    self.assertRaises(NoSuchElementException,
                      self.driver.find_elment_by_name, "q2")
    # Try to find the Google search button using the multiple find method.
    q = self.driver.find_elements_by_name("q")
    self.assertTrue(isinstance(q, list))
    self.assertTrue(len(q), 1)
    self.assertTrue(isinstance(q[0], WebElement))
    # Try finding something not on page, with multiple find an empty array
    # should return and no exception thrown.
    q = self.driver.find_elements_by_name("q2")
    assertTrue(q == [])
    # Find a hidden element on the page
    q = self.driver.find_element_by_name("oq")
    self.assertTrue(isinstance(q, WebElement))

 def testFindElementById(self):
    self.navigate(SEARCH)
    # Find the padding for the logo near the search bar.
    elem = self.driver.find_element_by_id("logocont")
    self.assertTrue(isinstance(elem, WebElement))
    # Look for an ID not there.
    self.assertRaises(NoSuchElementException,
                      self.driver.find_element_by_id, "logocont")

  def testFindElementById0WithTimeout(self):
    self.set_implicit_wait(0)
    self.navigate(SEARCH)
    # Find the padding for the logo near the search bar.
    elem = self.driver.find_element_by_id("logocont")
    self.assertTrue(isinstance(elem, WebElement))
    self.set_implicit_wait(5000)
    self.navigate(SEARCH)
    # Look for an ID not there.
    self.assertRaises(NoSuchElementException,
                      self.driver.find_element_by_id, "logocont")


class TestFindElement(RemoteWebDriverTest):
  def testFindByName(self):
    self.navigate(SEARCH)
    # Find the Google search button.
    q = self.driver.find_element_by_name("q")
    self.assertTrue(isinstance(q, WebElement))
    # Trying looking for an element not on the page.
    self.assertRaises(NoSuchElementException,
                      self.driver.find_elment_by_name, "q2")
    # Try to find the Google search button using the multiple find method.
    q = self.driver.find_elements_by_name("q")
    self.assertTrue(isinstance(q, list))
    self.assertTrue(len(q), 1)
    self.assertTrue(isinstance(q[0], WebElement))
    # Try finding something not on page, with multiple find an empty array
    # should return and no exception thrown.
    q = self.driver.find_elements_by_name("q2")
    assertTrue(q == [])
    # Find a hidden element on the page
    q = self.driver.find_element_by_name("oq")
    self.assertTrue(isinstance(q, WebElement))

 def testFindElementById(self):
    self.navigate(SEARCH)
    # Find the padding for the logo near the search bar.
    elem = self.driver.find_element_by_id("logocont")
    self.assertTrue(isinstance(elem, WebElement))
    # Look for an ID not there.
    self.assertRaises(NoSuchElementException,
                      self.driver.find_element_by_id, "logocont")


class TestJavaScriptExecution(RemoteWebDriverTest):
  """ Test the execute javascript ability of the remote driver"""
  def testNoModification(self):
    self.driver.get("http://www.google.com")
    title = self.driver.execute_script("return document.title")
    self.assertEqual(title, self.driver.get_title())

  def testModification(self):
    self.driver.get("http://www.google.com")
    title = self.driver.get_title()
    self.assertFind(title, "google")
    r = self.driver.execute_script("return document.Foo")
    self.assertTrue(r == None)
    self.driver.execute_script("document.Foo = \"Hello\"")
    r = self.driver.execute_script("return document.Foo")
    self.assertTrue(r == "Hello")

  def testComplexObjectFetch(self):
    self.driver.get("http://www.google.com")
    loc = self.driver.execute_script("return window.location")
    self.assertTrue(type(loc) is types.DictType)
    self.assertTrue(loc.has_key("hostname"))
    self.assertFind(loc["hostname"], "google")

  def testJavascriptExeception(self):
    self.driver.get("http://www.google.com")
    self.assertRaises(ErrorInResponseException, self.driver.execute_script,
                      "return windows");

  def testJavascriptWithNoReturn(self):
    self.driver.get("http://www.google.com")
    try:
      ret = self.driver.execute_script("return window.foobar")
      self.assertTrue(type(ret) is types.NoneType)
    except:
      self.assertTrue(False)


class TestNavigation(RemoteWebDriverTest):
  def testNavigateToURL(self):
    # No redirects are allowed on the google home page.
    self.navigate(self.SEARCH)

  def testGoBackWithNoHistory(self):
    # Go back one page with nothing to go back to.
    self.assertRaises(ErrorInResponseException, self.driver.back)

  def testGoForwardWithNoHistory(self):
    # Go forward with nothing to move forward to.
    self.assertRaises(ErrorInResponseException, self.driver.forward)

  def testNavigation(self):
    # Loads two pages into chrome's navigation history.
    self.navigate(self.NEWS)
    self.navigate(self.SEARCH)

    # Go back to news.
    self.driver.back()
    self.assertURL(self.NEWS)

    # Move forward to search.
    self.driver.forward()
    self.assertURL(self.SEARCH)

    # Verify refresh keeps us on the current page.
    self.driver.refresh()
    self.assertURL(self.SEARCH)

    # Go back to the previous URL and refresh making sure
    # we dont change the page.
    self.driver.back()
    self.assertURL(self.NEWS)
    self.driver.refresh()
    self.assertURL(self.NEWS)

  def testGetTitle(self):
    self.navigate(self.SEARCH)
    title = self.driver.get_title()
    # The google name must always be in the search title.
    self.assertFind(title, u"google")

  def testGetSource(self):
    self.navigate(self.SEARCH)
    source = self.driver.get_page_source()
    self.assertFind(source, u"document.body.style") # Basic javascript.
    self.assertFind(source, u"feeling lucky") # Lucky button.
    self.assertFind(source, u"google search") # Searh button.


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
  # Strip out our flags so unittest.main() correct parses the remaining.
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

  unittest.main()
