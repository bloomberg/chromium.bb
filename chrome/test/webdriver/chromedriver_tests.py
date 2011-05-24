#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for ChromeDriver.

If your test is testing a specific part of the WebDriver API, consider adding
it to the appropriate place in the WebDriver tree instead.
"""

import hashlib
import os
import platform
import sys
import unittest
import urllib
import urllib2
import urlparse

from chromedriver_launcher import ChromeDriverLauncher
import chromedriver_paths
from gtest_text_test_runner import GTestTextTestRunner

sys.path += [chromedriver_paths.SRC_THIRD_PARTY]
sys.path += [chromedriver_paths.PYTHON_BINDINGS]

try:
  import simplejson as json
except ImportError:
  import json

from selenium.webdriver.remote.command import Command
from selenium.webdriver.remote.webdriver import WebDriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities


def DataDir():
  """Returns the path to the data dir chrome/test/data."""
  return os.path.normpath(
    os.path.join(os.path.dirname(__file__), os.pardir, "data"))


def GetFileURLForPath(path):
  """Get file:// url for the given path.
  Also quotes the url using urllib.quote().
  """
  abs_path = os.path.abspath(path)
  if sys.platform == 'win32':
    # Don't quote the ':' in drive letter ( say, C: ) on win.
    # Also, replace '\' with '/' as expected in a file:/// url.
    drive, rest = os.path.splitdrive(abs_path)
    quoted_path = drive.upper() + urllib.quote((rest.replace('\\', '/')))
    return 'file:///' + quoted_path
  else:
    quoted_path = urllib.quote(abs_path)
    return 'file://' + quoted_path


def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def IsLinux():
  return sys.platform.startswith('linux')


def IsMac():
  return sys.platform.startswith('darwin')


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
    self._launcher = ChromeDriverLauncher()

  def tearDown(self):
    self._launcher.Kill()

  def testShouldReturn403WhenSentAnUnknownCommandURL(self):
    request_url = self._launcher.GetURL() + '/foo'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)

  def testShouldReturnHTTP405WhenSendingANonPostToTheSessionURL(self):
    request_url = self._launcher.GetURL() + '/session'
    try:
      SendRequest(request_url, method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 405')
    except urllib2.HTTPError, expected:
      self.assertEquals(405, expected.code)
      self.assertEquals('POST', expected.hdrs['Allow'])

  def testShouldGetA404WhenAttemptingToDeleteAnUnknownSession(self):
    request_url = self._launcher.GetURL() + '/session/unkown_session_id'
    try:
      SendRequest(request_url, method='DELETE')
      self.fail('Should have raised a urllib.HTTPError for returned 404')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testShouldReturn204ForFaviconRequests(self):
    request_url = self._launcher.GetURL() + '/favicon.ico'
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

  def testCanStartChromeDriverOnSpecificPort(self):
    launcher = ChromeDriverLauncher(port=9520)
    self.assertEquals(9520, launcher.GetPort())
    driver = WebDriver(launcher.GetURL(), DesiredCapabilities.CHROME)
    driver.quit()
    launcher.Kill()


class WebserverTest(unittest.TestCase):
  """Tests the built-in ChromeDriver webserver."""

  def testShouldNotServeFilesByDefault(self):
    launcher = ChromeDriverLauncher()
    try:
      SendRequest(launcher.GetURL(), method='GET')
      self.fail('Should have raised a urllib.HTTPError for returned 403')
    except urllib2.HTTPError, expected:
      self.assertEquals(403, expected.code)
    finally:
      launcher.Kill()

  def testCanServeFiles(self):
    launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))
    request_url = launcher.GetURL() + '/' + os.path.basename(__file__)
    SendRequest(request_url, method='GET')
    launcher.Kill()


class NativeInputTest(unittest.TestCase):
  """Native input ChromeDriver tests."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))
    self._capabilities = DesiredCapabilities.CHROME
    self._capabilities['chrome.nativeEvents'] = True

  def tearDown(self):
    self._launcher.Kill()

  def testCanStartWithNativeEvents(self):
    driver = WebDriver(self._launcher.GetURL(), self._capabilities)
    self.assertTrue(driver.capabilities.has_key('chrome.nativeEvents'))
    self.assertTrue(driver.capabilities['chrome.nativeEvents'])

  # Flaky on windows. See crbug.com/80295.
  def DISABLED_testSendKeysNative(self):
    driver = WebDriver(self._launcher.GetURL(), self._capabilities)
    driver.get(self._launcher.GetURL() + '/test_page.html')
    # Find the text input.
    q = driver.find_element_by_name('key_input_test')
    # Send some keys.
    q.send_keys('tokyo')
    self.assertEqual(q.text, 'tokyo')

  # Needs to run on a machine with an IME installed.
  def DISABLED_testSendKeysNativeProcessedByIME(self):
    driver = WebDriver(self._launcher.GetURL(), self.capabilities)
    driver.get(self._launcher.GetURL() + '/test_page.html')
    q = driver.find_element_by_name('key_input_test')
    # Send key combination to turn IME on.
    q.send_keys(Keys.F7)
    q.send_keys('toukyou')
    # Now turning it off.
    q.send_keys(Keys.F7)
    self.assertEqual(q.value, "\xe6\x9d\xb1\xe4\xba\xac")


class DesiredCapabilitiesTest(unittest.TestCase):

  def setUp(self):
    self._launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))

  def tearDown(self):
    self._launcher.Kill()

  def testCustomSwitches(self):
    switches = ['enable-file-cookie', 'homepage=about:memory']
    capabilities = {'chrome.switches': switches}

    driver = WebDriver(self._launcher.GetURL(), capabilities)
    url = driver.current_url
    self.assertTrue('memory' in url,
                    'URL does not contain with "memory":' + url)
    driver.get('about:version')
    self.assertNotEqual(-1, driver.page_source.find('enable-file-cookie'))

  def testBinary(self):
    binary_path = ChromeDriverLauncher.LocateExe()
    self.assertNotEquals(None, binary_path)
    if IsWindows():
      chrome_name = 'chrome.exe'
    elif IsMac():
      chrome_name = 'Google Chrome.app/Contents/MacOS/Google Chrome'
      if not os.path.exists(os.path.join(binary_path, chrome_name)):
        chrome_name = 'Chromium.app/Contents/MacOS/Chromium'
    elif IsLinux():
      chrome_name = 'chrome'
    else:
      self.fail('Unrecognized platform: ' + sys.platform)
    binary_path = os.path.join(os.path.dirname(binary_path), chrome_name)
    self.assertTrue(os.path.exists(binary_path),
                    'Binary not found: ' + binary_path)
    capabilities = {'chrome.binary': binary_path}

    driver = WebDriver(self._launcher.GetURL(), capabilities)


class CookieTest(unittest.TestCase):
  """Cookie test for the json webdriver protocol"""

  def setUp(self):
    self._launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))
    self._driver = WebDriver(self._launcher.GetURL(),
                             DesiredCapabilities.CHROME)

  def tearDown(self):
    self._driver.quit()
    self._launcher.Kill()

  def testAddCookie(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    cookie_dict = None
    cookie_dict = self._driver.get_cookie("chromedriver_cookie_test")
    cookie_dict = {}
    cookie_dict["name"]= "chromedriver_cookie_test"
    cookie_dict["value"] = "this is a test"
    self._driver.add_cookie(cookie_dict)
    cookie_dict = self._driver.get_cookie("chromedriver_cookie_test")
    self.assertNotEqual(cookie_dict, None)
    self.assertEqual(cookie_dict["value"], "this is a test")

  def testDeleteCookie(self):
    self.testAddCookie();
    self._driver.delete_cookie("chromedriver_cookie_test")
    cookie_dict = self._driver.get_cookie("chromedriver_cookie_test")
    self.assertEqual(cookie_dict, None)


class ScreenshotTest(unittest.TestCase):
  """Tests to verify screenshot retrieval"""

  REDBOX = "automation_proxy_snapshot/set_size.html"

  def setUp(self):
    self._launcher = ChromeDriverLauncher()
    self._driver = WebDriver(self._launcher.GetURL(), {})

  def tearDown(self):
    self._driver.quit()
    self._launcher.Kill()

  def testScreenCaptureAgainstReference(self):
    # Create a red square of 2000x2000 pixels.
    url = GetFileURLForPath(os.path.join(DataDir(),
                                         self.REDBOX))
    url += "?2000,2000"
    self._driver.get(url)
    s = self._driver.get_screenshot_as_base64();
    self._driver.get_screenshot_as_file("/tmp/foo.png")
    h = hashlib.md5(s).hexdigest()
    # Compare the PNG created to the reference hash.
    self.assertEquals(h, '12c0ade27e3875da3d8866f52d2fa84f')


class SessionTest(unittest.TestCase):
  """Tests dealing with WebDriver sessions."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher()

  def tearDown(self):
    self._launcher.Kill()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._launcher.GetURL() + '/session'
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

  def testShouldBeGivenCapabilitiesWhenStartingASession(self):
    driver = WebDriver(self._launcher.GetURL(), {})
    capabilities = driver.capabilities

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
    driver.quit()

  def testSessionCreationDeletion(self):
    driver = WebDriver(self._launcher.GetURL(), DesiredCapabilities.CHROME)
    driver.quit()

  def testMultipleSessionCreationDeletion(self):
    for i in range(10):
      driver = WebDriver(self._launcher.GetURL(), DesiredCapabilities.CHROME)
      driver.quit()

  def testSessionCommandsAfterSessionDeletionReturn404(self):
    driver = WebDriver(self._launcher.GetURL(), DesiredCapabilities.CHROME)
    session_id = driver.session_id
    driver.quit()
    try:
      response = SendRequest(self._launcher.GetURL() + '/session/' + session_id,
                             method='GET')
      self.fail('Should have thrown 404 exception')
    except urllib2.HTTPError, expected:
      self.assertEquals(404, expected.code)

  def testMultipleConcurrentSessions(self):
    drivers = []
    for i in range(10):
      drivers += [WebDriver(self._launcher.GetURL(),
                            DesiredCapabilities.CHROME)]
    for driver in drivers:
      driver.quit()


class MouseTest(unittest.TestCase):
  """Mouse command tests for the json webdriver protocol"""

  def setUp(self):
    self._launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))
    self._driver = WebDriver(self._launcher.GetURL(),
                             DesiredCapabilities.CHROME)

  def tearDown(self):
    self._driver.quit()
    self._launcher.Kill()

  def testClickElementThatNeedsContainerScrolling(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    self._driver.find_element_by_name('hidden_scroll').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testClickElementThatNeedsIframeScrolling(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    self._driver.switch_to_frame('iframe')
    self._driver.find_element_by_name('hidden_scroll').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testClickElementThatNeedsPageScrolling(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    self._driver.find_element_by_name('far_away').click()
    self.assertTrue(self._driver.execute_script('return window.success'))

  def testDoNotScrollUnnecessarilyToClick(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    self._driver.find_element_by_name('near_top').click()
    self.assertTrue(self._driver.execute_script('return window.success'))
    script = 'return document.body.scrollTop == 0 && ' \
             '       document.body.scrollLeft == 0'
    self.assertTrue(self._driver.execute_script(script))


class UrlBaseTest(unittest.TestCase):
  """Tests that the server can be configured for a different URL base."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher(url_base='/wd/hub')

  def tearDown(self):
    self._launcher.Kill()

  def testCreatingSessionShouldRedirectToCorrectURL(self):
    request_url = self._launcher.GetURL() + '/session'
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
class ElementEqualityTest(unittest.TestCase):
  """Tests that the server properly checks element equality."""

  def setUp(self):
    self._launcher = ChromeDriverLauncher(root_path=os.path.dirname(__file__))
    self._driver = WebDriver(self._launcher.GetURL(), {})

  def tearDown(self):
    self._driver.quit()
    self._launcher.Kill()

  def testElementEquality(self):
    self._driver.get(self._launcher.GetURL() + '/test_page.html')
    body1 = self._driver.find_element_by_tag_name('body')
    body2 = self._driver.execute_script('return document.body')

    # TODO(jleyba): WebDriver's python bindings should expose a proper API
    # for this.
    result = body1._execute(Command.ELEMENT_EQUALS, {
      'other': body2.id
    })
    self.assertTrue(result['value'])


"""Chrome functional test section. All implementation tests of ChromeDriver
should go above.

TODO(dyu): Move these tests out of here when pyauto has these capabilities.
"""


def GetPathForDataFile(relative_path):
  """Returns the path for a test data file residing in this directory."""
  return os.path.join(os.path.dirname(__file__), relative_path)


class AutofillTest(unittest.TestCase):
  AUTOFILL_EDIT_ADDRESS = 'chrome://settings/autofillEditAddress'
  AUTOFILL_EDIT_CC = 'chrome://settings/autofillEditCreditCard'

  def setUp(self):
    self._launcher = ChromeDriverLauncher()

  def tearDown(self):
    self._launcher.Kill()

  def NewDriver(self):
    return WebDriver(self._launcher.GetURL(), {})

  def _SelectOptionXpath(self, value):
    """Returns an xpath query used to select an item from a dropdown list.

    Args:
      value: Option selected for the drop-down list field.
    """
    return '//option[@value="%s"]' % value

  def testPostalCodeAndStateLabelsBasedOnCountry(self):
    """Verify postal code and state labels based on selected country."""
    import simplejson
    test_data = simplejson.loads(
        open(GetPathForDataFile('state_zip_labels.txt')).read())

    driver = self.NewDriver()
    driver.get(self.AUTOFILL_EDIT_ADDRESS)
    # Initial check of State and ZIP labels.
    state_label = driver.find_element_by_id('state-label').text
    self.assertEqual('State', state_label)
    zip_label = driver.find_element_by_id('postal-code-label').text
    self.assertEqual('ZIP code', zip_label)

    for country_code in test_data:
      query = self._SelectOptionXpath(country_code)
      driver.find_element_by_id('country').find_element_by_xpath(query).select()
      # Compare postal labels.
      actual_postal_label = driver.find_element_by_id(
          'postal-code-label').text
      expected_postal_label = test_data[country_code]['postalCodeLabel']
      self.assertEqual(
          actual_postal_label, expected_postal_label,
          'Postal code label does not match Country "%s"' % country_code)
      # Compare state labels.
      actual_state_label = driver.find_element_by_id('state-label').text
      expected_state_label = test_data[country_code]['stateLabel']
      self.assertEqual(
          actual_state_label, expected_state_label,
          'State label does not match Country "%s"' % country_code)

  def testDisplayLineItemForEntriesWithNoCCNum(self):
    """Verify Autofill creates a line item for CC entries with no CC number."""
    creditcard_data = {'CREDIT_CARD_NAME': 'Jane Doe',
                       'CREDIT_CARD_EXP_MONTH': '12',
                       'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2014'}

    driver = self.NewDriver()
    driver.get(self.AUTOFILL_EDIT_CC)
    driver.find_element_by_id('name-on-card').send_keys(
        creditcard_data['CREDIT_CARD_NAME'])
    query_month = self._SelectOptionXpath(
        creditcard_data['CREDIT_CARD_EXP_MONTH'])
    query_year = self._SelectOptionXpath(
        creditcard_data['CREDIT_CARD_EXP_4_DIGIT_YEAR'])
    driver.find_element_by_id('expiration-month').find_element_by_xpath(
        query_month).select()
    driver.find_element_by_id('expiration-year').find_element_by_xpath(
        query_year).select()
    driver.find_element_by_id(
        'autofill-edit-credit-card-apply-button').click()
    # Refresh the page to ensure the UI is up-to-date.
    driver.refresh()
    list_entry = driver.find_element_by_class_name('autofill-list-item')
    self.assertTrue(list_entry.is_displayed)
    self.assertEqual(list_entry.text,
                     creditcard_data['CREDIT_CARD_NAME'],
                     'Saved CC line item not same as what was entered.')


if __name__ == '__main__':
  unittest.main(module='chromedriver_tests',
                testRunner=GTestTextTestRunner(verbosity=1))
