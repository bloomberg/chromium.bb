# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

from selenium.common.exceptions import NoSuchWindowException
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.remote.command import Command
from selenium.webdriver.remote.webdriver import WebDriver
from selenium.webdriver.support.ui import WebDriverWait


def SkipIf(should_skip):
  """Decorator which allows skipping individual test cases."""
  if should_skip:
    return lambda func: None
  return lambda func: func


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


class BasicTest(ChromeDriverTest):
  """Basic ChromeDriver tests."""

  def setUp(self):
    self._server2 = ChromeDriverLauncher(self.GetDriverPath()).Launch()

  def tearDown(self):
    self._server2.Kill()

  def testShouldReturn403WhenSentAnUnknownCommandURL(self):
    request_url = self._server2.GetUrl() + '/foo'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)

  def testShouldReturnHTTP405WhenSendingANonPostToTheSessionURL(self):
    request_url = self._server2.GetUrl() + '/session'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 405')
    except urllib2.HTTPError, expected:
      self.assertEquals(405, expected.code)
      self.assertEquals('POST', expected.hdrs['Allow'])

  def testShouldGetA404WhenAttemptingToDeleteAnUnknownSession(self):
    request_url = self._server2.GetUrl() + '/session/unkown_session_id'
    try:
      SendRequest(request_url, method='DELETE')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testShouldReturn204ForFaviconRequests(self):
    request_url = self._server2.GetUrl() + '/favicon.ico'
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
    request_url = self._server2.GetUrl() + '/session'
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


class WebserverTest(ChromeDriverTest):
  """Tests the built-in ChromeDriver webserver."""

  def testShouldNotServeFilesByDefault(self):
    server = ChromeDriverLauncher(self.GetDriverPath()).Launch()
    try:
      SendRequest(server.GetUrl(), method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)
    finally:
      server.Kill()

  def testCanServeFiles(self):
    launcher = ChromeDriverLauncher(self.GetDriverPath(),
                                    root_path=os.path.dirname(__file__))
    server = launcher.Launch()
    request_url = server.GetUrl() + '/' + os.path.basename(__file__)
    SendRequest(request_url, method='GET')
    server.Kill()


class DesiredCapabilitiesTest(ChromeDriverTest):
  """Tests for webdriver desired capabilities."""

  def testCustomSwitches(self):
    switches = ['enable-file-cookie']
    capabilities = {'chrome.switches': switches}

    driver = self.GetNewDriver(capabilities)
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
    extension_names = [x.get_name() for x in driver.get_installed_extensions()]
    self.assertEquals(2, len(extension_names))
    self.assertTrue('ExtTest1' in extension_names)
    self.assertTrue('ExtTest2' in extension_names)
    driver.quit()

  def testPrefs(self):
    """Test that chromedriver can set user preferences."""
    driver = self.GetNewDriver({
      'chrome.noWebsiteTestingDefaults': True,
      'chrome.prefs': {
        'profile.default_content_settings': {
          'popups': 1
        },
      }
    })
    driver.get(self.GetTestDataUrl() + '/empty.html')
    driver.execute_script('window.open("about:blank")')
    self.assertEquals(2, len(driver.window_handles))

  def testLoadAsync(self):
    """Test that chromedriver can load pages asynchronously."""
    driver = self.GetNewDriver({'chrome.loadAsync': True})

    # Check that navigate doesn't wait for the page to load.
    driver.get(self.GetTestDataUrl() + '/hang')

    # Check that the navigation actually starts.
    def IsEmptyPage(driver):
      return driver.current_url.endswith('empty.html')
    driver.get(self.GetTestDataUrl() + '/empty.html')
    WebDriverWait(driver, 10).until(IsEmptyPage)


class DetachProcessTest(ChromeDriverTest):

  def setUp(self):
    self._server2 = ChromeDriverLauncher(self.GetDriverPath()).Launch()
    self._factory2 = ChromeDriverFactory(self._server2)

  def tearDown(self):
    self._server2.Kill()

  # TODO(kkania): Remove this when Chrome 15 is stable.
  # crbug.com/134982
  def DISABLED_testDetachProcess(self):
    # This is a weak test. Its purpose is to just make sure we can start
    # Chrome successfully in detached mode. There's not an easy way to know
    # if Chrome is shutting down due to the channel error when the client
    # disconnects.
    driver = self._factory2.GetNewDriver({'chrome.detach': True})
    driver.get('about:memory')
    pid = int(driver.find_elements_by_xpath('//*[@jscontent="pid"]')[0].text)
    self._server2.Kill()
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

  # crbug.com/136875
  def DISABLED_testClickElementThatNeedsContainerScrolling(self):
    self._driver.get(self.GetTestDataUrl() + '/test_page.html')
    self._driver.find_element_by_name('hidden_scroll').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  # crbug.com/136875
  def DISABLED_testClickElementThatNeedsIframeScrolling(self):
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


# crbug.com/109698: when running in xvfb, 2 extra mouse moves are received.
# crbug.com/138125: fails if the mouse cursor is left over the page.
@SkipIf(True)
class MouseEventTest(ChromeDriverTest):
  """Tests for checking the correctness of mouse events."""

  def setUp(self):
    super(MouseEventTest, self).setUp()
    self._driver = self.GetNewDriver()
    ActionChains(self._driver).key_down([Keys.CONTROL, Keys.SHIFT]).perform()
    self._driver.get(self.GetTestDataUrl() + '/events.html')
    self._divs = self._driver.find_elements_by_tag_name('div')

  def _CheckEvent(self, event, event_type, mouse_button, x, y):
    """Checks the given event properties.

    This function expects the ctrl and shift keys to be pressed.
    """
    self.assertEquals(event_type, event['type'])
    self.assertEquals(mouse_button, event['button'])
    self.assertEquals(False, event['altKey'])
    self.assertEquals(True, event['ctrlKey'])
    self.assertEquals(True, event['shiftKey'])
    self.assertEquals(x, event['x'])
    self.assertEquals(y, event['y'])

  def _GetElementMiddle(self, elem):
    x = elem.location['x']
    y = elem.location['y']
    return (x + (elem.size['width'] + 1) / 2, y + (elem.size['height'] + 1) / 2)

  def testMoveCommand(self):
    x = self._divs[0].location['x']
    y = self._divs[0].location['y']
    center_x, center_y = self._GetElementMiddle(self._divs[0])

    # Move to element.
    ActionChains(self._driver).move_to_element(self._divs[0]).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(1, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, center_x, center_y)

    # Move by offset.
    ActionChains(self._driver).move_by_offset(1, 2).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(1, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, center_x + 1, center_y + 2)

    # Move to element and offset.
    ActionChains(self._driver).move_to_element_with_offset(
        self._divs[0], 2, 1).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(1, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, x + 2, y + 1)

  def testClickCommand(self):
    center_x, center_y = self._GetElementMiddle(self._divs[0])

    # Left click element.
    ActionChains(self._driver).click(self._divs[0]).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(3, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, center_x, center_y)
    self._CheckEvent(events[1], 'mousedown', 0, center_x, center_y)
    self._CheckEvent(events[2], 'mouseup', 0, center_x, center_y)

    # Left click.
    ActionChains(self._driver).click(None).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(2, len(events))
    self._CheckEvent(events[0], 'mousedown', 0, center_x, center_y)
    self._CheckEvent(events[1], 'mouseup', 0, center_x, center_y)

    # Right click.
    ActionChains(self._driver).context_click(None).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(2, len(events))
    self._CheckEvent(events[0], 'mousedown', 2, center_x, center_y)
    self._CheckEvent(events[1], 'mouseup', 2, center_x, center_y)

  def testButtonDownUpCommand(self):
    center_x, center_y = self._GetElementMiddle(self._divs[0])
    center_x2, center_y2 = self._GetElementMiddle(self._divs[1])

    # Press and release element.
    ActionChains(self._driver).click_and_hold(self._divs[0]).release(
        self._divs[1]).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(4, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, center_x, center_y)
    self._CheckEvent(events[1], 'mousedown', 0, center_x, center_y)
    self._CheckEvent(events[2], 'mousemove', 0, center_x2, center_y2)
    self._CheckEvent(events[3], 'mouseup', 0, center_x2, center_y2)

    # Press and release.
    ActionChains(self._driver).click_and_hold(None).release(None).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(2, len(events))
    self._CheckEvent(events[0], 'mousedown', 0, center_x2, center_y2)
    self._CheckEvent(events[1], 'mouseup', 0, center_x2, center_y2)

  def testDoubleClickCommand(self):
    center_x, center_y = self._GetElementMiddle(self._divs[0])

    # Double click element.
    ActionChains(self._driver).double_click(self._divs[0]).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(6, len(events))
    self._CheckEvent(events[5], 'dblclick', 0, center_x, center_y)

    # Double click.
    ActionChains(self._driver).double_click(None).perform()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(5, len(events))
    self._CheckEvent(events[4], 'dblclick', 0, center_x, center_y)

  def testElementAPIClick(self):
    center_x, center_y = self._GetElementMiddle(self._divs[0])

    # Left click element.
    self._divs[0].click()
    events = self._driver.execute_script('return takeEvents()')
    self.assertEquals(3, len(events))
    self._CheckEvent(events[0], 'mousemove', 0, center_x, center_y)
    self._CheckEvent(events[1], 'mousedown', 0, center_x, center_y)
    self._CheckEvent(events[2], 'mouseup', 0, center_x, center_y)


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

  def testWithWebWidgets(self):
    def SetHTML(html):
      """Sets the page HTML.

      The given HTML should not contain single quotes.
      """
      assert '\'' not in html
      self._driver.execute_script('document.body.innerHTML = \'%s\'' % html)
    SetHTML('<input type="checkbox">check</input>')
    elem = self._driver.find_element_by_tag_name('input')
    elem.send_keys(' ')
    self.assertTrue(elem.is_selected())
    elem.send_keys(' ')
    self.assertFalse(elem.is_selected())

    SetHTML('<input type="radio" name="g" checked>1</input>' +
            '<input type="radio" name="g">2</input>')
    elem1, elem2 = self._driver.find_elements_by_tag_name('input')
    elem1.send_keys(Keys.RIGHT)
    self.assertTrue(elem2.is_selected())
    elem2.send_keys(Keys.LEFT)
    self.assertFalse(elem2.is_selected())

    SetHTML('<select><option>a</option><option>b</option></select>')
    elem = self._driver.find_element_by_tag_name('select')
    elem.send_keys('b')
    self.assertEquals('b', elem.get_attribute('value'))

    handler = 'javascript:document.title=\\x27success\\x27'
    SetHTML('<input type="button" onclick="%s"></input>' % handler)
    elem = self._driver.find_element_by_tag_name('input')
    elem.send_keys(' ')
    self.assertEquals('success', self._driver.title)


class UrlBaseTest(ChromeDriverTest):
  """Tests that the server can be configured for a different URL base."""

  def setUp(self):
    self._server2 = ChromeDriverLauncher(self.GetDriverPath(),
                                         url_base='/wd/hub').Launch()

  def tearDown(self):
    self._server2.Kill()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._server2.GetUrl() + '/session'
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


class LoggingTest(ChromeDriverTest):

  def testLogging(self):
    url = self.GetServer().GetUrl()
    req = SendRequest(url + '/log', method='GET')
    log = req.read()
    self.assertTrue('INFO' in log, msg='INFO not in log: ' + log)


class FileUploadControlTest(ChromeDriverTest):
  """Tests dealing with file upload control."""

  def setUp(self):
    super(FileUploadControlTest, self).setUp()
    self._driver = self.GetNewDriver()

  # Fails on win - crbug.com/131782
  def DISABLED_testSetFilePathToFileUploadControl(self):
    """Verify a file path is set to the file upload control."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    tmp_file = tempfile.NamedTemporaryFile()

    fileupload_single = self._driver.find_element_by_name('fileupload_single')
    multiple = fileupload_single.get_attribute('multiple')
    self.assertEqual('false', multiple)
    fileupload_single.send_keys(tmp_file.name)
    path = fileupload_single.get_attribute('value')
    self.assertTrue(path.endswith(os.path.basename(tmp_file.name)))

  def testSetMultipleFilePathsToFileuploadControlWithoutMultipleWillFail(self):
    """Verify setting file paths to the file upload control without 'multiple'
    attribute will fail."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    files = []
    filepaths = []
    for index in xrange(4):
      tmp_file = tempfile.NamedTemporaryFile()
      # We need to hold the file objects because the files will be deleted on
      # GC.
      files.append(tmp_file)
      filepath = tmp_file.name
      filepaths.append(filepath)

    fileupload_single = self._driver.find_element_by_name('fileupload_single')
    self.assertFalse(fileupload_single.get_attribute('multiple'))
    self.assertRaises(WebDriverException, fileupload_single.send_keys,
                      '\n'.join(filepaths))

  def testSetMultipleFilePathsToFileUploadControl(self):
    """Verify multiple file paths are set to the file upload control."""
    self._driver.get(self.GetTestDataUrl() + '/upload.html')

    files = []
    filepaths = []
    filenames = set()
    for index in xrange(4):
      tmp_file = tempfile.NamedTemporaryFile()
      files.append(tmp_file)
      filepath = tmp_file.name
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
    old_window = driver.current_window_handle
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
    self.assertRaises(WebDriverException, driver.get,
                      self.GetTestDataUrl() + '/alert_on_load.html')
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

  # See http://code.google.com/p/selenium/issues/detail?id=2671.
  def testCanPerformJSBasedActionsThatCauseAlertsAtTheEnd(self):
    driver = self.GetNewDriver()
    driver.execute_script(
        'var select = document.createElement("select");' +
        'select.innerHTML = "<option>1</option><option>2</option>";' +
        'select.addEventListener("change", function() { alert("hi"); });' +
        'document.body.appendChild(select);')

    # Shouldn't throw an exception, even though an alert appears mid-script.
    driver.find_elements_by_tag_name('option')[-1].click()

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


class WindowTest(ChromeDriverTest):
  """Tests for WebDriver window commands."""

  def setUp(self):
    super(WindowTest, self).setUp()
    self._driver = self.GetNewDriver()

  def testSize(self):
    size = self._driver.get_window_size()
    self._driver.set_window_size(size['width'], size['height'])
    self.assertEquals(size, self._driver.get_window_size())
    self._driver.set_window_size(800, 600)
    self.assertEquals(800, self._driver.get_window_size()['width'])
    self.assertEquals(600, self._driver.get_window_size()['height'])

  def testPosition(self):
    pos = self._driver.get_window_position()
    self._driver.set_window_position(pos['x'], pos['y'])
    self.assertEquals(pos, self._driver.get_window_position())
    self._driver.set_window_position(100, 200)
    self.assertEquals(100, self._driver.get_window_position()['x'])
    self.assertEquals(200, self._driver.get_window_position()['y'])

  # Systems without window manager (Xvfb, Xvnc) do not implement maximization.
  @SkipIf(util.IsLinux())
  def testMaximize(self):
    old_size = self._driver.get_window_size()
    self._driver.maximize_window()
    new_size = self._driver.get_window_size()
    self.assertTrue(old_size['width'] <= new_size['width'])
    self.assertTrue(old_size['height'] <= new_size['height'])

  def testWindowHandle(self):
    """Test specifying window handle."""
    self._driver.execute_script(
        'window.open("about:blank", "name", "height=200, width=200")')
    windows = self._driver.window_handles
    self.assertEquals(2, len(windows))
    self._driver.set_window_size(400, 300, windows[1])
    self.assertEquals(400, self._driver.get_window_size(windows[1])['width'])
    self.assertEquals(300, self._driver.get_window_size(windows[1])['height'])
    self.assertNotEquals(self._driver.get_window_size(windows[1]),
                         self._driver.get_window_size(windows[0]))

  def testInvalidWindowHandle(self):
    """Tests specifying invalid handle."""
    invalid_handle = 'f1-120'
    self.assertRaises(WebDriverException, self._driver.set_window_size,
                      400, 300, invalid_handle)
    self.assertRaises(NoSuchWindowException, self._driver.get_window_size,
                      invalid_handle)
    self.assertRaises(NoSuchWindowException, self._driver.set_window_position,
                      1, 1, invalid_handle)
    self.assertRaises(NoSuchWindowException, self._driver.get_window_position,
                      invalid_handle)


class GeolocationTest(ChromeDriverTest):
  """Tests for WebDriver geolocation commands."""

  def testGeolocation(self):
    """Tests the get and set geolocation commands."""
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/empty.html')

    # TODO(kkania): Update the python bindings and get rid of these.
    driver.command_executor._commands.update({
        'getLoc': ('GET', '/session/$sessionId/location'),
        'setLoc': ('POST', '/session/$sessionId/location')
    })
    def getLocation():
      return driver.execute('getLoc')['value']
    def setLocation(location):
      driver.execute('setLoc', {'location': location})
    expected_location = {'latitude': 50, 'longitude': 50, 'altitude': 300}
    setLocation(expected_location)
    location = getLocation()
    self.assertEquals(expected_location, location)

    driver.set_script_timeout(10)
    result = driver.execute_async_script("""
       var callback = arguments[0];
       window.navigator.geolocation.getCurrentPosition(
           function success(result) { callback(result.coords); },
           function fail(error) { callback(error.message); });""")
    self.assertEquals(expected_location['latitude'], result['latitude'])
    self.assertEquals(expected_location['longitude'], result['longitude'])
    self.assertEquals(expected_location['altitude'], result['altitude'])


class ExtensionTest(ChromeDriverTest):

  INFOBAR_BROWSER_ACTION_EXTENSION = test_paths.TEST_DATA_PATH + \
      '/infobar_browser_action_extension'
  PAGE_ACTION_EXTENSION = test_paths.TEST_DATA_PATH + \
      '/page_action_extension'
  APP_SHELL = test_paths.TEST_DATA_PATH + \
      '/app_shell_extension'

  def testExtensionInstallAndUninstall(self):
    driver = self.GetNewDriver()
    self.assertEquals(0, len(driver.get_installed_extensions()))
    ext = driver.install_extension(self.PAGE_ACTION_EXTENSION)
    extensions = driver.get_installed_extensions()
    self.assertEquals(1, len(extensions))
    self.assertEquals(ext.id, extensions[0].id)
    ext.uninstall()
    self.assertEquals(0, len(driver.get_installed_extensions()))

  def testExtensionInfo(self):
    driver = self.GetNewDriver()
    ext = driver.install_extension(self.PAGE_ACTION_EXTENSION)
    self.assertEquals('Page action extension', ext.get_name())
    self.assertEquals('1.0', ext.get_version())
    self.assertEquals(32, len(ext.id))
    self.assertTrue(ext.is_enabled())
    ext.set_enabled(True)
    ext.set_enabled(False)
    self.assertFalse(ext.is_enabled())
    ext.set_enabled(True)
    self.assertTrue(ext.is_enabled())

  def _testExtensionView(self, driver, view_handle, extension):
    """Tests that the given view supports basic WebDriver functionality."""
    driver.switch_to_window(view_handle)
    self.assertTrue(driver.execute_script('return true'))
    checkbox = driver.find_element_by_id('checkbox')
    checkbox.click()
    self.assertTrue(checkbox.is_selected())
    textfield = driver.find_element_by_id('textfield')
    textfield.send_keys('test')
    self.assertEquals('test', textfield.get_attribute('value'))
    self.assertEquals('test', driver.title)
    self.assertTrue(driver.current_url.endswith('view_checks.html'))
    self.assertTrue('Should be in page source' in driver.page_source)
    driver.close()
    def is_view_closed(driver):
      return len(filter(lambda view: view['handle'] == view_handle,
                        extension._get_views())) == 0
    WebDriverWait(driver, 10).until(is_view_closed)

  # Mac extension infobars are currently broken: crbug.com/107573.
  @SkipIf(util.IsMac())
  def testInfobarView(self):
    driver = self.GetNewDriver({'chrome.switches':
                                ['enable-experimental-extension-apis']})
    ext = driver.install_extension(self.INFOBAR_BROWSER_ACTION_EXTENSION)
    driver.switch_to_window(ext.get_bg_page_handle())
    driver.set_script_timeout(10)
    driver.execute_async_script('waitForInfobar(arguments[0])')
    self._testExtensionView(driver, ext.get_infobar_handles()[0], ext)

  def testBrowserActionPopupView(self):
    driver = self.GetNewDriver({'chrome.switches':
                                ['enable-experimental-extension-apis']})
    ext = driver.install_extension(self.INFOBAR_BROWSER_ACTION_EXTENSION)
    ext.click_browser_action()
    self._testExtensionView(driver, ext.get_popup_handle(), ext)

  def testPageActionPopupView(self):
    driver = self.GetNewDriver()
    ext = driver.install_extension(self.PAGE_ACTION_EXTENSION)
    def is_page_action_visible(driver):
      return ext.is_page_action_visible()
    WebDriverWait(driver, 10).until(is_page_action_visible)
    ext.click_page_action()
    self._testExtensionView(driver, ext.get_popup_handle(), ext)

  def testAppShellView(self):
    driver = self.GetNewDriver({'chrome.switches':
                                ['enable-experimental-extension-apis']})
    ext = driver.install_extension(self.APP_SHELL)

    # Navigates to the new tab page to launch the app.
    driver.get('chrome:newtab')
    app = driver.find_element_by_xpath("//div[@title='App Shell']")
    app.click()
    def is_app_window_launched(driver):
      return ext.get_app_shell_handle() is not None
    WebDriverWait(driver, 10).until(is_app_window_launched)
    self._testExtensionView(driver, ext.get_app_shell_handle(), ext)


class BadJSTest(ChromeDriverTest):
  """Tests that ensure sites with hacky JS don't break ChromeDriver."""

  def testFindElementDoesNotUseNativeFuncs(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/bad_native_funcs.html')
    # This will throw an exception if any native funcs are used.
    driver.find_element_by_tag_name('body').find_elements_by_tag_name('div')


class ContentSettingsTest(ChromeDriverTest):
  """Tests that various types of content are allowed by default."""

  def testPopups(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/empty.html')
    driver.execute_script('window.open("about:blank")')
    self.assertEquals(2, len(driver.window_handles))

  # Failing on win7: crbug.com/141231.
  @SkipIf(util.IsWin())
  def testPopupsCanBeResized(self):
    """Regression test for chromedriver issue 126."""
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/empty.html')
    driver.execute_script(
        'window.open("empty.html", "popup", "width=500,height=500")')
    driver.switch_to_window(driver.window_handles[1])
    size = driver.get_window_size()
    bigger_size = dict(map(lambda x: (x, size[x] + 100), size))
    smaller_size = dict(map(lambda x: (x, size[x] - 100), size))
    driver.set_window_size(bigger_size['width'], bigger_size['height'])
    self.assertEquals(bigger_size, driver.get_window_size())
    driver.set_window_size(smaller_size['width'], smaller_size['height'])
    self.assertEquals(smaller_size, driver.get_window_size())

  def testGeolocation(self):
    driver = self.GetNewDriver()
    driver.get(self.GetTestDataUrl() + '/empty.html')
    driver.set_script_timeout(10)
    # Will timeout if infobar appears.
    driver.execute_async_script(
        'navigator.geolocation.getCurrentPosition(arguments[0], arguments[0]);')

  def testMediaStream(self):
    driver = self.GetNewDriver()
    # Allowing camera/mic access by default only works for https sites.
    driver.get(self.GetHttpsTestDataUrl() + '/empty.html')
    driver.set_script_timeout(10)
    # Will timeout if infobar appears.
    driver.execute_async_script(
        'navigator.webkitGetUserMedia({audio:true, video:true},' +
        '                             arguments[0], arguments[0]);')
