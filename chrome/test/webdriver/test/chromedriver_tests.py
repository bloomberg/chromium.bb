# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for ChromeDriver.

If your test is testing a specific part of the WebDriver API, consider adding
it to the appropriate place in the WebDriver tree instead.
"""

import binascii
from distutils import archive_util
import hashlib
import httplib
import os
import platform
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib
import urllib2
import urlparse

from chromedriver_factory import ChromeDriverFactory
from chromedriver_launcher import ChromeDriverLauncher
from chromedriver_test import ChromeDriverTest
import test_paths
import util

try:
  import simplejson as json
except ImportError:
  import json

from selenium.common.exceptions import WebDriverException
from selenium.webdriver.remote.command import Command
from selenium.webdriver.remote.webdriver import WebDriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities


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


class BasicTest(unittest.TestCase):
  """Basic ChromeDriver tests."""

  def setUp(self):
    self._server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE).Launch()

  def tearDown(self):
    self._server.Kill()

  def testShouldReturn403WhenSentAnUnknownCommandURL(self):
    request_url = self._server.GetUrl() + '/foo'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)

  def testShouldReturnHTTP405WhenSendingANonPostToTheSessionURL(self):
    request_url = self._server.GetUrl() + '/session'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 405')
    except urllib2.HTTPError, expected:
      self.assertEquals(405, expected.code)
      self.assertEquals('POST', expected.hdrs['Allow'])

  def testShouldGetA404WhenAttemptingToDeleteAnUnknownSession(self):
    request_url = self._server.GetUrl() + '/session/unkown_session_id'
    try:
      SendRequest(request_url, method='DELETE')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testShouldReturn204ForFaviconRequests(self):
    request_url = self._server.GetUrl() + '/favicon.ico'
    # In python2.5, a 204 status code causes an exception.
    if sys.version_info[0:2] == (2, 5):
      try:
        SendRequest(request_url, method='GET')
        self.fail('Should have raised a urllib.HTTPError for returned 204')
      except urllib2.HTTPError, expected:
        self.assertEquals(204, expected.code)
    else:
      response = SendRequest(request_url, method='GET')
      try:
        self.assertEquals(204, response.code)
      finally:
        response.close()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._server.GetUrl() + '/session'
    response = SendRequest(request_url, method='POST',
                           data='{"desiredCapabilities": {}}')
    self.assertEquals(200, response.code)
    self.session_url = response.geturl()  # TODO(jleyba): verify this URL?

    data = json.loads(response.read())
    self.assertTrue(isinstance(data, dict))
    self.assertEquals(0, data['status'])

    url_parts = urlparse.urlparse(self.session_url)[2].split('/')
    self.assertEquals(3, len(url_parts))
    self.assertEquals('', url_parts[0])
    self.assertEquals('session', url_parts[1])
    self.assertEquals(data['sessionId'], url_parts[2])


class WebserverTest(unittest.TestCase):
  """Tests the built-in ChromeDriver webserver."""

  def testShouldNotServeFilesByDefault(self):
    server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE).Launch()
    try:
      SendRequest(server.GetUrl(), method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)
    finally:
      server.Kill()

  def testCanServeFiles(self):
    launcher = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE,
                                    root_path=os.path.dirname(__file__))
    server = launcher.Launch()
    request_url = server.GetUrl() + '/' + os.path.basename(__file__)
    SendRequest(request_url, method='GET')
    server.Kill()


class NativeInputTest(ChromeDriverTest):
  """Native input ChromeDriver tests."""

  _CAPABILITIES = {'chrome.nativeEvents': True }

  def testCanStartWithNativeEvents(self):
    driver = self.GetNewDriver(NativeInputTest._CAPABILITIES)
    self.assertTrue(driver.capabilities['chrome.nativeEvents'])

  # Flaky on windows. See crbug.com/80295.
  def DISABLED_testSendKeysNative(self):
    driver = self.GetNewDriver(NativeInputTest._CAPABILITIES)
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    # Find the text input.
    q = driver.find_element_by_name('key_input_test')
    # Send some keys.
    q.send_keys('tokyo')
    self.assertEqual(q.text, 'tokyo')

  # Needs to run on a machine with an IME installed.
  def DISABLED_testSendKeysNativeProcessedByIME(self):
    driver = self.GetNewDriver(NativeInputTest._CAPABILITIES)
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    q = driver.find_element_by_name('key_input_test')
    # Send key combination to turn IME on.
    q.send_keys(Keys.F7)
    q.send_keys('toukyou')
    # Now turning it off.
    q.send_keys(Keys.F7)
    self.assertEqual(q.get_attribute('value'), "\xe6\x9d\xb1\xe4\xba\xac")


class DesiredCapabilitiesTest(ChromeDriverTest):
  """Tests for webdriver desired capabilities."""

  def testCustomSwitches(self):
    switches = ['enable-file-cookie', 'homepage=about:memory']
    capabilities = {'chrome.switches': switches}

    driver = self.GetNewDriver(capabilities)
    url = driver.current_url
    self.assertTrue('memory' in url,
                    'URL does not contain with "memory":' + url)
    driver.get('about:version')
    self.assertNotEqual(-1, driver.page_source.find('enable-file-cookie'))
    driver.quit()

  def testBinary(self):
    self.GetNewDriver({'chrome.binary': self.GetChromePath()})

  def testUserProfile(self):
    """Test starting WebDriver session with custom profile."""

    # Open a new session and save the user profile.
    profile_dir = tempfile.mkdtemp()
    capabilities = {'chrome.switches': ['--user-data-dir=' + profile_dir]}
    driver = self.GetNewDriver(capabilities)
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    # Create a cookie.
    cookie_dict = {}
    cookie_dict['name'] = 'test_user_profile'
    cookie_dict['value'] = 'chrome profile'
    cookie_dict['expiry'] = time.time() + 120
    driver.add_cookie(cookie_dict)
    driver.quit()

    profile_zip = archive_util.make_archive(os.path.join(profile_dir,
                                                         'profile'),
                                            'zip',
                                            root_dir=profile_dir,
                                            base_dir='Default')
    f = open(profile_zip, 'rb')
    base64_user_profile = binascii.b2a_base64(f.read()).strip()
    f.close()
    os.remove(profile_zip)

    # Start new session with the saved user profile.
    capabilities = {'chrome.profile': base64_user_profile}
    driver = self.GetNewDriver(capabilities)
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    cookie_dict = driver.get_cookie('test_user_profile')
    self.assertNotEqual(cookie_dict, None)
    self.assertEqual(cookie_dict['value'], 'chrome profile')
    driver.quit()

  def testInstallExtensions(self):
    """Test starting web driver with multiple extensions."""
    extensions = ['ext_test_1.crx', 'ext_test_2.crx']
    base64_extensions = []
    for ext in extensions:
      f = open(test_paths.GetTestDataPath(ext), 'rb')
      base64_ext = (binascii.b2a_base64(f.read()).strip())
      base64_extensions.append(base64_ext)
      f.close()
    capabilities = {'chrome.extensions': base64_extensions}
    driver = self.GetNewDriver(capabilities)
    # Assert the extensions are installed.
    driver.get('chrome://extensions/')
    self.assertNotEqual(-1, driver.page_source.find('ExtTest1'))
    self.assertNotEqual(-1, driver.page_source.find('ExtTest2'))
    driver.quit()


class DetachProcessTest(unittest.TestCase):

  def setUp(self):
    self._server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE).Launch()
    self._factory = ChromeDriverFactory(self._server)

  def tearDown(self):
    self._server.Kill()

  # TODO(kkania): Remove this when Chrome 15 is stable.
  def testDetachProcess(self):
    # This is a weak test. Its purpose is to just make sure we can start
    # Chrome successfully in detached mode. There's not an easy way to know
    # if Chrome is shutting down due to the channel error when the client
    # disconnects.
    driver = self._factory.GetNewDriver({'chrome.detach': True})
    driver.get('about:memory')
    pid = int(driver.find_elements_by_xpath('//*[@jscontent="pid"]')[0].text)
    self._server.Kill()
    try:
      util.Kill(pid)
    except OSError:
      self.fail('Chrome quit after detached chromedriver server was killed')


class CookieTest(ChromeDriverTest):
  """Cookie test for the json webdriver protocol"""

  def testAddCookie(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    cookie_dict = None
    cookie_dict = driver.get_cookie("chromedriver_cookie_test")
    cookie_dict = {}
    cookie_dict["name"] = "chromedriver_cookie_test"
    cookie_dict["value"] = "this is a test"
    driver.add_cookie(cookie_dict)
    cookie_dict = driver.get_cookie("chromedriver_cookie_test")
    self.assertNotEqual(cookie_dict, None)
    self.assertEqual(cookie_dict["value"], "this is a test")

  def testDeleteCookie(self):
    driver = self.GetNewDriver()
    self.testAddCookie();
    driver.delete_cookie("chromedriver_cookie_test")
    cookie_dict = driver.get_cookie("chromedriver_cookie_test")
    self.assertEqual(cookie_dict, None)


class ScreenshotTest(ChromeDriverTest):
  """Tests to verify screenshot retrieval"""

  REDBOX = "automation_proxy_snapshot/set_size.html"

  def testScreenCaptureAgainstReference(self):
    # Create a red square of 2000x2000 pixels.
    url = util.GetFileURLForPath(test_paths.GetChromeTestDataPath(self.REDBOX))
    url += '?2000,2000'
    driver = self.GetNewDriver()
    driver.get(url)
    s = driver.get_screenshot_as_base64()
    h = hashlib.md5(s).hexdigest()
    # Compare the PNG created to the reference hash.
    self.assertEquals(h, '12c0ade27e3875da3d8866f52d2fa84f')

  # This test requires Flash and must be run on a VM or via remote desktop.
  # See crbug.com/96317.
  def testSnapshotWithWindowlessFlashAndTransparentOverlay(self):
    if not util.IsWin():
      return

    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/plugin_transparency_test.html')
    snapshot = driver.get_screenshot_as_base64()
    self.assertEquals(hashlib.md5(snapshot).hexdigest(),
                      '72e5b8525e48758bae59997472f27f14')


class SessionTest(ChromeDriverTest):
  """Tests dealing with WebDriver sessions."""

  def testShouldBeGivenCapabilitiesWhenStartingASession(self):
    driver = self.GetNewDriver()
    capabilities = driver.capabilities

    self.assertEquals('chrome', capabilities['browserName'])
    self.assertTrue(capabilities['javascriptEnabled'])
    self.assertTrue(capabilities['takesScreenshot'])
    self.assertTrue(capabilities['cssSelectorsEnabled'])

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

  def testSessionCreationDeletion(self):
    self.GetNewDriver().quit()

  # crbug.com/103396
  def DISABLED_testMultipleSessionCreationDeletion(self):
    for i in range(10):
      self.GetNewDriver().quit()

  def testSessionCommandsAfterSessionDeletionReturn404(self):
    driver = self.GetNewDriver()
    url = self.GetTestDataUrl()
    url += '/session/' + driver.session_id
    driver.quit()
    try:
      response = SendRequest(url, method='GET')
      self.fail('Should have thrown 404 exception')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testMultipleConcurrentSessions(self):
    drivers = []
    for i in range(10):
      drivers += [self.GetNewDriver()]
    for driver in drivers:
      driver.quit()


class ShutdownTest(ChromeDriverTest):

  def setUp(self):
    super(ShutdownTest, self).setUp()
    self._custom_server = ChromeDriverLauncher(self.GetDriverPath()).Launch()
    self._custom_factory = ChromeDriverFactory(self._custom_server,
                                               self.GetChromePath())

  def tearDown(self):
    self._custom_server.Kill()
    super(ShutdownTest, self).tearDown()

  def testShutdownWithSession(self):
    driver = self._custom_factory.GetNewDriver()
    driver.get(self._custom_server.GetUrl() + '/status')
    driver.find_element_by_tag_name('body')
    self._custom_server.Kill()

  def testShutdownWithBusySession(self):
    def _Hang(driver):
      """Waits for the process to quit and then notifies."""
      try:
        driver.get(self._custom_server.GetUrl() + '/hang')
      except httplib.BadStatusLine:
        pass

    driver = self._custom_factory.GetNewDriver()
    wait_thread = threading.Thread(target=_Hang, args=(driver,))
    wait_thread.start()
    wait_thread.join(5)
    self.assertTrue(wait_thread.isAlive())

    self._custom_server.Kill()
    wait_thread.join(10)
    self.assertFalse(wait_thread.isAlive())


class MouseTest(ChromeDriverTest):
  """Mouse command tests for the json webdriver protocol"""

  def setUp(self):
    super(MouseTest, self).setUp()
    self._driver = self.GetNewDriver()

  def testCanClickTransparentElement(self):
    self._driver.get(self.GetTestDataUrl() + '/transparent.html')
    self._driver.find_element_by_tag_name('a').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testClickElementThatNeedsContainerScrolling(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.find_element_by_name('hidden_scroll').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testClickElementThatNeedsIframeScrolling(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.switch_to_frame('iframe')
    self._driver.find_element_by_name('hidden_scroll').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testClickElementThatNeedsPageScrolling(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.find_element_by_name('far_away').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  # TODO(kkania): Move this test to the webdriver repo.
  def testClickDoesSelectOption(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    option = self._driver.find_element_by_name('option')
    self.assertFalse(option.is_selected())
    option.click()
    self.assertTrue(option.is_selected())

  def testClickDoesUseFirstClientRect(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.find_element_by_name('wrapped').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testThrowErrorIfNotClickable(self):
    self._driver.get(self.GetTestDataUrl() + '/not_clickable.html')
    elem = self._driver.find_element_by_name('click')
    self.assertRaises(WebDriverException, elem.click)


class TypingTest(ChromeDriverTest):

  def setUp(self):
    super(TypingTest, self).setUp()
    self._driver = self.GetNewDriver()

  def testSendKeysToEditingHostDiv(self):
    self._driver.get(self.GetTestDataUrl() + '/content_editable.html')
    div = self._driver.find_element_by_name('editable')
    # Break into two to ensure element doesn't lose focus.
    div.send_keys('hi')
    div.send_keys(' there')
    self.assertEquals('hi there', div.text)

  def testSendKeysToNonFocusableChildOfEditingHost(self):
    self._driver.get(self.GetTestDataUrl() + '/content_editable.html')
    child = self._driver.find_element_by_name('editable_child')
    self.assertRaises(WebDriverException, child.send_keys, 'hi')

  def testSendKeysToFocusableChildOfEditingHost(self):
    self._driver.get(self.GetTestDataUrl() + '/content_editable.html')
    child = self._driver.find_element_by_tag_name('input')
    child.send_keys('hi')
    child.send_keys(' there')
    self.assertEquals('hi there', child.get_attribute('value'))

  def testSendKeysToDesignModePage(self):
    self._driver.get(self.GetTestDataUrl() + '/design_mode_doc.html')
    body = self._driver.find_element_by_tag_name('body')
    body.send_keys('hi')
    body.send_keys(' there')
    self.assertEquals('hi there', body.text)

  def testSendKeysToDesignModeIframe(self):
    self._driver.get(self.GetTestDataUrl() + '/content_editable.html')
    self._driver.switch_to_frame(0)
    body = self._driver.find_element_by_tag_name('body')
    body.send_keys('hi')
    body.send_keys(' there')
    self.assertEquals('hi there', body.text)

  def testSendKeysToTransparentElement(self):
    self._driver.get(self.GetTestDataUrl() + '/transparent.html')
    text_box = self._driver.find_element_by_tag_name('input')
    text_box.send_keys('hi')
    self.assertEquals('hi', text_box.get_attribute('value'))

  def testSendKeysDesignModePageAfterNavigate(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.get(self.GetTestDataUrl() + '/design_mode_doc.html')
    body = self._driver.find_element_by_tag_name('body')
    body.send_keys('hi')
    body.send_keys(' there')
    self.assertEquals('hi there', body.text)

  def testAppendsToTextInput(self):
    self._driver.get(self.GetTestDataUrl() + '/keyboard.html')
    text_elem = self._driver.find_element_by_name('input')
    text_elem.send_keys(' text')
    self.assertEquals('more text', text_elem.get_attribute('value'))
    area_elem = self._driver.find_element_by_name('area')
    area_elem.send_keys(' text')
    self.assertEquals('more text', area_elem.get_attribute('value'))

  def testTextAreaKeepsCursorPosition(self):
    self._driver.get(self.GetTestDataUrl() + '/keyboard.html')
    area_elem = self._driver.find_element_by_name('area')
    area_elem.send_keys(' text')
    area_elem.send_keys(Keys.LEFT * 9)
    area_elem.send_keys('much ')
    self.assertEquals('much more text', area_elem.get_attribute('value'))


class UrlBaseTest(unittest.TestCase):
  """Tests that the server can be configured for a different URL base."""

  def setUp(self):
    self._server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE,
                                        url_base='/wd/hub').Launch()

  def tearDown(self):
    self._server.Kill()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._server.GetUrl() + '/session'
    response = SendRequest(request_url, method='POST',
                           data='{"desiredCapabilities":{}}')
    self.assertEquals(200, response.code)
    self.session_url = response.geturl()  # TODO(jleyba): verify this URL?

    data = json.loads(response.read())
    self.assertTrue(isinstance(data, dict))
    self.assertEquals(0, data['status'])

    url_parts = urlparse.urlparse(self.session_url)[2].split('/')
    self.assertEquals(5, len(url_parts))
    self.assertEquals('', url_parts[0])
    self.assertEquals('wd', url_parts[1])
    self.assertEquals('hub', url_parts[2])
    self.assertEquals('session', url_parts[3])
    self.assertEquals(data['sessionId'], url_parts[4])


# TODO(jleyba): Port this to WebDriver's own python test suite.
class ElementEqualityTest(ChromeDriverTest):
  """Tests that the server properly checks element equality."""

  def setUp(self):
    super(ElementEqualityTest, self).setUp()
    self._driver = self.GetNewDriver()

  def tearDown(self):
    self._driver.quit()

  def testElementEquality(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    body1 = self._driver.find_element_by_tag_name('body')
    body2 = self._driver.execute_script('return document.body')

    # TODO(jleyba): WebDriver's python bindings should expose a proper API
    # for this.
    result = body1._execute(Command.ELEMENT_EQUALS, {
      'other': body2.id
    })
    self.assertTrue(result['value'])


class LoggingTest(unittest.TestCase):

  def setUp(self):
    self._server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE).Launch()
    self._factory = ChromeDriverFactory(self._server)

  def tearDown(self):
    self._factory.QuitAll()
    self._server.Kill()

  def testNoVerboseLogging(self):
    driver = self._factory.GetNewDriver()
    url = self._factory.GetServer().GetUrl()
    driver.execute_script('console.log("HI")')
    req = SendRequest(url + '/log', method='GET')
    log = req.read()
    self.assertTrue(':INFO:' not in log, ':INFO: in log: ' + log)

  # crbug.com/94470
  def DISABLED_testVerboseLogging(self):
    driver = self._factory.GetNewDriver({'chrome.verbose': True})
    url = self._factory.GetServer().GetUrl()
    driver.execute_script('console.log("HI")')
    req = SendRequest(url + '/log', method='GET')
    log = req.read()
    self.assertTrue(':INFO:' in log, ':INFO: not in log: ' + log)


class FileUploadControlTest(ChromeDriverTest):
  """Tests dealing with file upload control."""

  def setUp(self):
    super(FileUploadControlTest, self).setUp()
    self._driver = self.GetNewDriver()

  def testSetFilePathToFileUploadControl(self):
    """Verify a file path is set to the file upload control."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    file = tempfile.NamedTemporaryFile()

    fileupload_single = self._driver.find_element_by_name('fileupload_single')
    multiple = fileupload_single.get_attribute('multiple')
    self.assertEqual('false', multiple)
    fileupload_single.send_keys(file.name)
    path = fileupload_single.get_attribute('value')
    self.assertTrue(path.endswith(os.path.basename(file.name)))

  def testSetMultipleFilePathsToFileuploadControlWithoutMultipleWillFail(self):
    """Verify setting file paths to the file upload control without 'multiple'
    attribute will fail."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    files = []
    filepaths = []
    for index in xrange(4):
      file = tempfile.NamedTemporaryFile()
      # We need to hold the file objects because the files will be deleted on
      # GC.
      files.append(file)
      filepath = file.name
      filepaths.append(filepath)

    fileupload_single = self._driver.find_element_by_name('fileupload_single')
    multiple = fileupload_single.get_attribute('multiple')
    self.assertEqual('false', multiple)
    self.assertRaises(WebDriverException, fileupload_single.send_keys,
                      '\n'.join(filepaths))

  def testSetMultipleFilePathsToFileUploadControl(self):
    """Verify multiple file paths are set to the file upload control."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    files = []
    filepaths = []
    filenames = set()
    for index in xrange(4):
      file = tempfile.NamedTemporaryFile()
      files.append(file)
      filepath = file.name
      filepaths.append(filepath)
      filenames.add(os.path.basename(filepath))

    fileupload_multi = self._driver.find_element_by_name('fileupload_multi')
    multiple = fileupload_multi.get_attribute('multiple')
    self.assertEqual('true', multiple)
    fileupload_multi.send_keys('\n'.join(filepaths))

    files_on_element = self._driver.execute_script(
        'return document.getElementById("fileupload_multi").files;')
    self.assertTrue(files_on_element)
    self.assertEqual(4, len(files_on_element))
    for f in files_on_element:
      self.assertTrue(f['name'] in filenames)


class FrameSwitchingTest(ChromeDriverTest):

  def testGetWindowHandles(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    driver.execute_script('window.popup = window.open("about:blank")')
    self.assertEquals(2, len(driver.window_handles))
    driver.execute_script('window.popup.close()')
    self.assertEquals(1, len(driver.window_handles))

  def testSwitchToSameWindow(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    driver.switch_to_window(driver.window_handles[0])
    self.assertEquals('test_page.html', driver.current_url.split('/')[-1])

  def testClosedWindowThrows(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    driver.execute_script('window.open("about:blank")')
    driver.close()
    self.assertRaises(WebDriverException, driver.close)

  def testSwitchFromClosedWindow(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    driver.execute_script('window.open("about:blank")')
    driver.close()
    driver.switch_to_window(driver.window_handles[0])
    self.assertEquals('about:blank', driver.current_url)

  def testSwitchToWindowWhileInSubframe(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/test_page.html')
    driver.execute_script('window.open("about:blank")')
    driver.switch_to_frame(0)
    driver.switch_to_window(driver.window_handles[1])
    self.assertEquals('about:blank', driver.current_url)

  # Tests that the indexing is absolute and not based on index of frame in its
  # parent element.
  # See crbug.com/88685.
  def testSwitchToFrameByIndex(self):
    driver = self.GetNewDriver({'chrome.switches': ['disable-popup-blocking']})
    driver.get(self.GetTestDataUrl() + '/switch_to_frame_by_index.html')
    for i in range(3):
      driver.switch_to_frame(i)
      self.assertEquals(str(i), driver.current_url.split('?')[-1])
      driver.switch_to_default_content()

class AlertTest(ChromeDriverTest):

  def testAlertOnLoadDoesNotHang(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alert_on_load.html')
    driver.switch_to_alert().accept()

  def testAlertWhenTypingThrows(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alerts.html')
    input_box = driver.find_element_by_name('onkeypress')
    self.assertRaises(WebDriverException, input_box.send_keys, 'a')

  def testAlertJustAfterTypingDoesNotThrow(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alerts.html')
    driver.find_element_by_name('onkeyup').send_keys('a')
    driver.switch_to_alert().accept()

  def testAlertOnScriptDoesNotHang(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alerts.html')
    self.assertRaises(WebDriverException, driver.execute_script, 'alert("ok")')

  def testMustHandleAlertFirst(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alerts.html')
    input_box = driver.find_element_by_name('normal')
    driver.execute_async_script('arguments[0](); window.alert("ok")')

    self.assertRaises(WebDriverException, driver.execute_script, 'a = 1')

    self.assertRaises(WebDriverException, input_box.send_keys, 'abc')

    self.assertRaises(WebDriverException, driver.get,
                      self.GetTestDataUrl() + '/test_page.html')

    self.assertRaises(WebDriverException, driver.refresh)
    self.assertRaises(WebDriverException, driver.back)
    self.assertRaises(WebDriverException, driver.forward)
    self.assertRaises(WebDriverException, driver.get_screenshot_as_base64)

  def testCanHandleAlertInSubframe(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/alerts.html')
    driver.switch_to_frame('subframe')
    driver.execute_async_script('arguments[0](); window.alert("ok")')
    driver.switch_to_alert().accept()
