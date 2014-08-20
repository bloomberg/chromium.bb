#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""End to end tests for ChromeDriver."""

import base64
import json
import math
import optparse
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib2

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(1, os.path.join(_THIS_DIR, os.pardir))
sys.path.insert(1, os.path.join(_THIS_DIR, os.pardir, 'client'))
sys.path.insert(1, os.path.join(_THIS_DIR, os.pardir, 'server'))

import chrome_paths
import chromedriver
import unittest_util
import util
import server
from webelement import WebElement
import webserver

_TEST_DATA_DIR = os.path.join(chrome_paths.GetTestData(), 'chromedriver')

if util.IsLinux():
  sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'android'))
  from pylib import android_commands
  from pylib import constants
  from pylib import forwarder
  from pylib import valgrind_tools
  from pylib.device import device_utils


_NEGATIVE_FILTER = [
    # https://code.google.com/p/chromedriver/issues/detail?id=213
    'ChromeDriverTest.testClickElementInSubFrame',
    # This test is flaky since it uses setTimeout.
    # Re-enable once crbug.com/177511 is fixed and we can remove setTimeout.
    'ChromeDriverTest.testAlert',
]

_VERSION_SPECIFIC_FILTER = {}
_VERSION_SPECIFIC_FILTER['HEAD'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=815
    'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
]

_OS_SPECIFIC_FILTER = {}
_OS_SPECIFIC_FILTER['win'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=214
    'ChromeDriverTest.testCloseWindow',
    # https://code.google.com/p/chromedriver/issues/detail?id=299
    'ChromeLogPathCapabilityTest.testChromeLogPath',
]
_OS_SPECIFIC_FILTER['linux'] = [
    # Xvfb doesn't support maximization.
    'ChromeDriverTest.testWindowMaximize',
    # https://code.google.com/p/chromedriver/issues/detail?id=302
    'ChromeDriverTest.testWindowPosition',
    'ChromeDriverTest.testWindowSize',
]
_OS_SPECIFIC_FILTER['mac'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=304
    'ChromeDriverTest.testGoBackAndGoForward',
]

_DESKTOP_NEGATIVE_FILTER = [
    # Desktop doesn't support touch (without --touch-events).
    'ChromeDriverTest.testSingleTapElement',
    'ChromeDriverTest.testTouchDownUpElement',
    'ChromeDriverTest.testTouchFlickElement',
    'ChromeDriverTest.testTouchMovedElement',
    'ChromeDriverAndroidTest.*',
]


def _GetDesktopNegativeFilter(version_name):
  filter = _NEGATIVE_FILTER + _DESKTOP_NEGATIVE_FILTER
  os = util.GetPlatformName()
  if os in _OS_SPECIFIC_FILTER:
    filter += _OS_SPECIFIC_FILTER[os]
  if version_name in _VERSION_SPECIFIC_FILTER:
    filter += _VERSION_SPECIFIC_FILTER[version_name]
  return filter

_ANDROID_NEGATIVE_FILTER = {}
_ANDROID_NEGATIVE_FILTER['chrome'] = (
    _NEGATIVE_FILTER + [
        # TODO(chrisgao): fix hang of tab crash test on android.
        'ChromeDriverTest.testTabCrash',
        # Android doesn't support switches and extensions.
        'ChromeSwitchesCapabilityTest.*',
        'ChromeExtensionsCapabilityTest.*',
        'MobileEmulationCapabilityTest.*',
        # https://crbug.com/274650
        'ChromeDriverTest.testCloseWindow',
        # https://code.google.com/p/chromedriver/issues/detail?id=270
        'ChromeDriverTest.testPopups',
        # https://code.google.com/p/chromedriver/issues/detail?id=298
        'ChromeDriverTest.testWindowPosition',
        'ChromeDriverTest.testWindowSize',
        'ChromeDriverTest.testWindowMaximize',
        'ChromeLogPathCapabilityTest.testChromeLogPath',
        'RemoteBrowserTest.*',
        # Don't enable perf testing on Android yet.
        'PerfTest.testSessionStartTime',
        'PerfTest.testSessionStopTime',
        'PerfTest.testColdExecuteScript',
        # https://code.google.com/p/chromedriver/issues/detail?id=459
        'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
    ]
)
_ANDROID_NEGATIVE_FILTER['chrome_stable'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'])
_ANDROID_NEGATIVE_FILTER['chrome_beta'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'])
_ANDROID_NEGATIVE_FILTER['chrome_shell'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'] + [
        # ChromeShell doesn't support multiple tabs.
        'ChromeDriverTest.testGetWindowHandles',
        'ChromeDriverTest.testSwitchToWindow',
        'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
    ]
)
_ANDROID_NEGATIVE_FILTER['chromedriver_webview_shell'] = (
    _ANDROID_NEGATIVE_FILTER['chrome_shell'])


class ChromeDriverBaseTest(unittest.TestCase):
  """Base class for testing chromedriver functionalities."""

  def __init__(self, *args, **kwargs):
    super(ChromeDriverBaseTest, self).__init__(*args, **kwargs)
    self._drivers = []

  def tearDown(self):
    for driver in self._drivers:
      try:
        driver.Quit()
      except:
        pass

  def CreateDriver(self, server_url=None, **kwargs):
    if server_url is None:
      server_url = _CHROMEDRIVER_SERVER_URL

    android_package = None
    android_activity = None
    android_process = None
    if _ANDROID_PACKAGE_KEY:
      android_package = constants.PACKAGE_INFO[_ANDROID_PACKAGE_KEY].package
      if _ANDROID_PACKAGE_KEY == 'chromedriver_webview_shell':
        android_activity = constants.PACKAGE_INFO[_ANDROID_PACKAGE_KEY].activity
        android_process = '%s:main' % android_package

    driver = chromedriver.ChromeDriver(server_url,
                                       chrome_binary=_CHROME_BINARY,
                                       android_package=android_package,
                                       android_activity=android_activity,
                                       android_process=android_process,
                                       **kwargs)
    self._drivers += [driver]
    return driver


class ChromeDriverTest(ChromeDriverBaseTest):
  """End to end tests for ChromeDriver."""

  @staticmethod
  def GlobalSetUp():
    ChromeDriverTest._http_server = webserver.WebServer(
        chrome_paths.GetTestData())
    ChromeDriverTest._sync_server = webserver.SyncWebServer()
    if _ANDROID_PACKAGE_KEY:
      ChromeDriverTest._device = device_utils.DeviceUtils(
          android_commands.GetAttachedDevices()[0])
      http_host_port = ChromeDriverTest._http_server._server.server_port
      sync_host_port = ChromeDriverTest._sync_server._server.server_port
      forwarder.Forwarder.Map(
          [(http_host_port, http_host_port), (sync_host_port, sync_host_port)],
          ChromeDriverTest._device)

  @staticmethod
  def GlobalTearDown():
    if _ANDROID_PACKAGE_KEY:
      forwarder.Forwarder.UnmapAllDevicePorts(ChromeDriverTest._device)
    ChromeDriverTest._http_server.Shutdown()

  @staticmethod
  def GetHttpUrlForFile(file_path):
    return ChromeDriverTest._http_server.GetUrl() + file_path

  def setUp(self):
    self._driver = self.CreateDriver()

  def testStartStop(self):
    pass

  def testLoadUrl(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))

  def testGetCurrentWindowHandle(self):
    self._driver.GetCurrentWindowHandle()

  def _WaitForNewWindow(self, old_handles):
    """Wait for at least one new window to show up in 20 seconds.

    Args:
      old_handles: Handles to all old windows before the new window is added.

    Returns:
      Handle to a new window. None if timeout.
    """
    timeout = time.time() + 20
    while time.time() < timeout:
      new_handles = self._driver.GetWindowHandles()
      if len(new_handles) > len(old_handles):
        for index, old_handle in enumerate(old_handles):
          self.assertEquals(old_handle, new_handles[index])
        return new_handles[len(old_handles)]
      time.sleep(0.01)
    return None

  def testCloseWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('id', 'link').Click()
    new_window_handle = self._WaitForNewWindow(old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEquals(new_window_handle, self._driver.GetCurrentWindowHandle())
    self.assertRaises(chromedriver.NoSuchElement,
                      self._driver.FindElement, 'id', 'link')
    self._driver.CloseWindow()
    self.assertRaises(chromedriver.NoSuchWindow,
                      self._driver.GetCurrentWindowHandle)
    new_handles = self._driver.GetWindowHandles()
    for old_handle in old_handles:
      self.assertTrue(old_handle in new_handles)
    for handle in new_handles:
      self._driver.SwitchToWindow(handle)
      self.assertEquals(handle, self._driver.GetCurrentWindowHandle())
      self._driver.CloseWindow()

  def testGetWindowHandles(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('id', 'link').Click()
    self.assertNotEqual(None, self._WaitForNewWindow(old_handles))

  def testSwitchToWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self.assertEquals(
        1, self._driver.ExecuteScript('window.name = "oldWindow"; return 1;'))
    window1_handle = self._driver.GetCurrentWindowHandle()
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('id', 'link').Click()
    new_window_handle = self._WaitForNewWindow(old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEquals(new_window_handle, self._driver.GetCurrentWindowHandle())
    self.assertRaises(chromedriver.NoSuchElement,
                      self._driver.FindElement, 'id', 'link')
    self._driver.SwitchToWindow('oldWindow')
    self.assertEquals(window1_handle, self._driver.GetCurrentWindowHandle())

  def testEvaluateScript(self):
    self.assertEquals(1, self._driver.ExecuteScript('return 1'))
    self.assertEquals(None, self._driver.ExecuteScript(''))

  def testEvaluateScriptWithArgs(self):
    script = ('document.body.innerHTML = "<div>b</div><div>c</div>";'
              'return {stuff: document.querySelectorAll("div")};')
    stuff = self._driver.ExecuteScript(script)['stuff']
    script = 'return arguments[0].innerHTML + arguments[1].innerHTML'
    self.assertEquals(
        'bc', self._driver.ExecuteScript(script, stuff[0], stuff[1]))

  def testEvaluateInvalidScript(self):
    self.assertRaises(chromedriver.ChromeDriverException,
                      self._driver.ExecuteScript, '{{{')

  def testExecuteAsyncScript(self):
    self._driver.SetTimeout('script', 3000)
    self.assertRaises(
        chromedriver.ScriptTimeout,
        self._driver.ExecuteAsyncScript,
        'var callback = arguments[0];'
        'setTimeout(function(){callback(1);}, 10000);')
    self.assertEquals(
        2,
        self._driver.ExecuteAsyncScript(
            'var callback = arguments[0];'
            'setTimeout(function(){callback(2);}, 300);'))

  def testSwitchToFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('id')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('name')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame(self._driver.FindElement('tag name', 'iframe'))
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))

  def testSwitchToParentFrame(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/nested.html'))
    self.assertTrue('One' in self._driver.GetPageSource())
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue('Two' in self._driver.GetPageSource())
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue('Three' in self._driver.GetPageSource())
    self._driver.SwitchToParentFrame()
    self.assertTrue('Two' in self._driver.GetPageSource())
    self._driver.SwitchToParentFrame()
    self.assertTrue('One' in self._driver.GetPageSource())

  def testExecuteInRemovedFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);'
        'window.addEventListener("message",'
        '    function(event) { document.body.removeChild(frame); });')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('id')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.ExecuteScript('parent.postMessage("remove", "*");')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))

  def testGetTitle(self):
    script = 'document.title = "title"; return 1;'
    self.assertEquals(1, self._driver.ExecuteScript(script))
    self.assertEquals('title', self._driver.GetTitle())

  def testGetPageSource(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self.assertTrue('Link to empty.html' in self._driver.GetPageSource())

  def testFindElement(self):
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    self.assertTrue(
        isinstance(self._driver.FindElement('tag name', 'div'), WebElement))

  def testFindElements(self):
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    divs = self._driver.FindElements('tag name', 'div')
    self.assertTrue(isinstance(divs, list))
    self.assertEquals(2, len(divs))
    for div in divs:
      self.assertTrue(isinstance(div, WebElement))

  def testFindChildElement(self):
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";')
    element = self._driver.FindElement('tag name', 'div')
    self.assertTrue(
        isinstance(element.FindElement('tag name', 'br'), WebElement))

  def testFindChildElements(self):
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><br></div>";')
    element = self._driver.FindElement('tag name', 'div')
    brs = element.FindElements('tag name', 'br')
    self.assertTrue(isinstance(brs, list))
    self.assertEquals(2, len(brs))
    for br in brs:
      self.assertTrue(isinstance(br, WebElement))

  def testHoverOverElement(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("mouseover", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return div;')
    div.HoverOver()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testClickElement(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("click", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    div.Click()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testSingleTapElement(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchend", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    div.SingleTap()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testTouchDownUpElement(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchend", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    loc = div.GetLocation()
    self._driver.TouchDown(loc['x'], loc['y'])
    self._driver.TouchUp(loc['x'], loc['y'])
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testTouchFlickElement(self):
    dx = 3
    dy = 4
    speed = 5
    flickTouchEventsPerSecond = 30
    moveEvents = int(
        math.sqrt(dx * dx + dy * dy) * flickTouchEventsPerSecond / speed)
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchstart", function() {'
        '  div.innerHTML = "preMove0";'
        '});'
        'div.addEventListener("touchmove", function() {'
        '  res = div.innerHTML.match(/preMove(\d+)/);'
        '  if (res != null) {'
        '    div.innerHTML = "preMove" + (parseInt(res[1], 10) + 1);'
        '  }'
        '});'
        'div.addEventListener("touchend", function() {'
        '  if (div.innerHTML == "preMove' + str(moveEvents) + '") {'
        '    div.innerHTML = "new<br>";'
        '  }'
        '});'
        'return div;')
    self._driver.TouchFlick(div, dx, dy, speed)
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testTouchMovedElement(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchmove", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    loc = div.GetLocation()
    self._driver.TouchDown(loc['x'], loc['y'])
    self._driver.TouchMove(loc['x'] + 1, loc['y'] + 1)
    self._driver.TouchUp(loc['x'] + 1, loc['y'] + 1)
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testClickElementInSubFrame(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/frame_test.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    # Test clicking element in the sub frame.
    self.testClickElement()

  def testClearElement(self):
    text = self._driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text" value="abc">\';'
        'var input = document.getElementsByTagName("input")[0];'
        'input.addEventListener("change", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return input;')
    text.Clear()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testSendKeysToElement(self):
    text = self._driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text">\';'
        'var input = document.getElementsByTagName("input")[0];'
        'input.addEventListener("change", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return input;')
    text.SendKeys('0123456789+-*/ Hi')
    text.SendKeys(', there!')
    value = self._driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEquals('0123456789+-*/ Hi, there!', value)

  def testGetCurrentUrl(self):
    self.assertEquals('data:,', self._driver.GetCurrentUrl())

  def testGoBackAndGoForward(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.GoBack()
    self._driver.GoForward()

  def testRefresh(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.Refresh()

  def testMouseMoveTo(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("mouseover", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div, 10, 10)
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testMouseClick(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("click", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div)
    self._driver.MouseClick()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testMouseButtonDownAndUp(self):
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("mousedown", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new1<br>";'
        '});'
        'div.addEventListener("mouseup", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new2<a></a>";'
        '});')
    self._driver.MouseMoveTo(None, 50, 50)
    self._driver.MouseButtonDown()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))
    self._driver.MouseButtonUp()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'a')))

  def testMouseDoubleClick(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("dblclick", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div, 1, 1)
    self._driver.MouseDoubleClick()
    self.assertEquals(1, len(self._driver.FindElements('tag name', 'br')))

  def testAlert(self):
    self.assertFalse(self._driver.IsAlertOpen())
    self._driver.ExecuteScript(
        'window.setTimeout('
        '    function() { window.confirmed = confirm(\'HI\'); },'
        '    0);')
    self.assertTrue(self._driver.IsAlertOpen())
    self.assertEquals('HI', self._driver.GetAlertMessage())
    self._driver.HandleAlert(False)
    self.assertFalse(self._driver.IsAlertOpen())
    self.assertEquals(False,
                      self._driver.ExecuteScript('return window.confirmed'))

  def testShouldHandleNewWindowLoadingProperly(self):
    """Tests that ChromeDriver determines loading correctly for new windows."""
    self._http_server.SetDataForPath(
        '/newwindow',
        """
        <html>
        <body>
        <a href='%s' target='_blank'>new window/tab</a>
        </body>
        </html>""" % self._sync_server.GetUrl())
    self._driver.Load(self._http_server.GetUrl() + '/newwindow')
    old_windows = self._driver.GetWindowHandles()
    self._driver.FindElement('tagName', 'a').Click()
    new_window = self._WaitForNewWindow(old_windows)
    self.assertNotEqual(None, new_window)

    self.assertFalse(self._driver.IsLoading())
    self._driver.SwitchToWindow(new_window)
    self.assertTrue(self._driver.IsLoading())
    self._sync_server.RespondWithContent('<html>new window</html>')
    self._driver.ExecuteScript('return 1')  # Shouldn't hang.

  def testPopups(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.ExecuteScript('window.open("about:blank")')
    new_window_handle = self._WaitForNewWindow(old_handles)
    self.assertNotEqual(None, new_window_handle)

  def testNoSuchFrame(self):
    self.assertRaises(chromedriver.NoSuchFrame,
                      self._driver.SwitchToFrame, 'nosuchframe')
    self.assertRaises(chromedriver.NoSuchFrame,
                      self._driver.SwitchToFrame,
                      self._driver.FindElement('tagName', 'body'))

  def testWindowPosition(self):
    position = self._driver.GetWindowPosition()
    self._driver.SetWindowPosition(position[0], position[1])
    self.assertEquals(position, self._driver.GetWindowPosition())

    # Resize so the window isn't moved offscreen.
    # See https://code.google.com/p/chromedriver/issues/detail?id=297.
    self._driver.SetWindowSize(300, 300)

    self._driver.SetWindowPosition(100, 200)
    self.assertEquals([100, 200], self._driver.GetWindowPosition())

  def testWindowSize(self):
    size = self._driver.GetWindowSize()
    self._driver.SetWindowSize(size[0], size[1])
    self.assertEquals(size, self._driver.GetWindowSize())

    self._driver.SetWindowSize(600, 400)
    self.assertEquals([600, 400], self._driver.GetWindowSize())

  def testWindowMaximize(self):
    self._driver.SetWindowPosition(100, 200)
    self._driver.SetWindowSize(600, 400)
    self._driver.MaximizeWindow()

    self.assertNotEqual([100, 200], self._driver.GetWindowPosition())
    self.assertNotEqual([600, 400], self._driver.GetWindowSize())
    # Set size first so that the window isn't moved offscreen.
    # See https://code.google.com/p/chromedriver/issues/detail?id=297.
    self._driver.SetWindowSize(600, 400)
    self._driver.SetWindowPosition(100, 200)
    self.assertEquals([100, 200], self._driver.GetWindowPosition())
    self.assertEquals([600, 400], self._driver.GetWindowSize())

  def testConsoleLogSources(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/console_log.html'))
    logs = self._driver.GetLog('browser')
    self.assertEquals(len(logs), 2)
    self.assertEquals(logs[0]['source'], 'network')
    self.assertEquals(logs[1]['source'], 'javascript')

  def testAutoReporting(self):
    self.assertFalse(self._driver.IsAutoReporting())
    self._driver.SetAutoReporting(True)
    self.assertTrue(self._driver.IsAutoReporting())
    url = self.GetHttpUrlForFile('/chromedriver/console_log.html')
    self.assertRaisesRegexp(chromedriver.UnknownError,
                            '.*(404|Failed to load resource).*',
                            self._driver.Load,
                            url)

  def testContextMenuEventFired(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/context_menu.html'))
    self._driver.MouseMoveTo(self._driver.FindElement('tagName', 'div'))
    self._driver.MouseClick(2)
    self.assertTrue(self._driver.ExecuteScript('return success'))

  def testHasFocusOnStartup(self):
    # Some pages (about:blank) cause Chrome to put the focus in URL bar.
    # This breaks tests depending on focus.
    self.assertTrue(self._driver.ExecuteScript('return document.hasFocus()'))

  def testTabCrash(self):
    # If a tab is crashed, the session will be deleted.
    # When 31 is released, will reload the tab instead.
    # https://code.google.com/p/chromedriver/issues/detail?id=547
    self.assertRaises(chromedriver.UnknownError,
                      self._driver.Load, 'chrome://crash')
    self.assertRaises(chromedriver.NoSuchSession,
                      self._driver.GetCurrentUrl)

  def testDoesntHangOnDebugger(self):
    self._driver.ExecuteScript('debugger;')

  def testMobileEmulationDisabledByDefault(self):
    self.assertFalse(self._driver.capabilities['mobileEmulationEnabled'])


class ChromeDriverAndroidTest(ChromeDriverBaseTest):
  """End to end tests for Android-specific tests."""

  def testLatestAndroidAppInstalled(self):
    if ('stable' not in _ANDROID_PACKAGE_KEY and
        'beta' not in _ANDROID_PACKAGE_KEY):
      return

    self._driver = self.CreateDriver()

    try:
      omaha_list = json.loads(
          urllib2.urlopen('http://omahaproxy.appspot.com/all.json').read())
      for l in omaha_list:
        if l['os'] != 'android':
          continue
        for v in l['versions']:
          if (('stable' in v['channel'] and 'stable' in _ANDROID_PACKAGE_KEY) or
              ('beta' in v['channel'] and 'beta' in _ANDROID_PACKAGE_KEY)):
            omaha = map(int, v['version'].split('.'))
            device = map(int, self._driver.capabilities['version'].split('.'))
            self.assertTrue(omaha <= device)
            return
      raise RuntimeError('Malformed omaha JSON')
    except urllib2.URLError as e:
      print 'Unable to fetch current version info from omahaproxy (%s)' % e

  def testDeviceManagement(self):
    self._drivers = [self.CreateDriver() for x in
                     android_commands.GetAttachedDevices()]
    self.assertRaises(chromedriver.UnknownError, self.CreateDriver)
    self._drivers[0].Quit()
    self._drivers[0] = self.CreateDriver()


class ChromeSwitchesCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes chromeOptions.args capabilities.

  Makes sure the switches are passed to Chrome.
  """

  def testSwitchWithoutArgument(self):
    """Tests that switch --dom-automation can be passed to Chrome.

    Unless --dom-automation is specified, window.domAutomationController
    is undefined.
    """
    driver = self.CreateDriver(chrome_switches=['dom-automation'])
    self.assertNotEqual(
        None,
        driver.ExecuteScript('return window.domAutomationController'))


class ChromeExtensionsCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes chromeOptions.extensions."""

  def _PackExtension(self, ext_path):
    return base64.b64encode(open(ext_path, 'rb').read())

  def testExtensionsInstall(self):
    """Checks that chromedriver can take the extensions in crx format."""
    crx_1 = os.path.join(_TEST_DATA_DIR, 'ext_test_1.crx')
    crx_2 = os.path.join(_TEST_DATA_DIR, 'ext_test_2.crx')
    self.CreateDriver(chrome_extensions=[self._PackExtension(crx_1),
                                         self._PackExtension(crx_2)])

  def testExtensionsInstallZip(self):
    """Checks that chromedriver can take the extensions in zip format."""
    zip_1 = os.path.join(_TEST_DATA_DIR, 'ext_test_1.zip')
    self.CreateDriver(chrome_extensions=[self._PackExtension(zip_1)])

  def testWaitsForExtensionToLoad(self):
    did_load_event = threading.Event()
    server = webserver.SyncWebServer()
    def RunServer():
      time.sleep(5)
      server.RespondWithContent('<html>iframe</html>')
      did_load_event.set()

    thread = threading.Thread(target=RunServer)
    thread.daemon = True
    thread.start()
    crx = os.path.join(_TEST_DATA_DIR, 'ext_slow_loader.crx')
    driver = self.CreateDriver(
        chrome_switches=['user-agent=' + server.GetUrl()],
        chrome_extensions=[self._PackExtension(crx)])
    self.assertTrue(did_load_event.is_set())


class ChromeLogPathCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes chromeOptions.logPath."""

  LOG_MESSAGE = 'Welcome to ChromeLogPathCapabilityTest!'

  def testChromeLogPath(self):
    """Checks that user can specify the path of the chrome log.

    Verifies that a log message is written into the specified log file.
    """
    tmp_log_path = tempfile.NamedTemporaryFile()
    driver = self.CreateDriver(chrome_log_path=tmp_log_path.name)
    driver.ExecuteScript('console.info("%s")' % self.LOG_MESSAGE)
    driver.Quit()
    self.assertTrue(self.LOG_MESSAGE in open(tmp_log_path.name).read())


class MobileEmulationCapabilityTest(ChromeDriverBaseTest):
  """Tests that ChromeDriver processes chromeOptions.mobileEmulation.

  Makes sure the device metrics are overridden in DevTools and user agent is
  overridden in Chrome.
  """

  @staticmethod
  def GlobalSetUp():
    def respondWithUserAgentString(request):
      return request.GetHeader('User-Agent')

    MobileEmulationCapabilityTest._http_server = webserver.WebServer(
        chrome_paths.GetTestData())
    MobileEmulationCapabilityTest._http_server.SetCallbackForPath(
        '/userAgent', respondWithUserAgentString)

  @staticmethod
  def GlobalTearDown():
    MobileEmulationCapabilityTest._http_server.Shutdown()

  def testDeviceMetrics(self):
    driver = self.CreateDriver(
        mobile_emulation = {
            'deviceMetrics': {'width': 360, 'height': 640, 'pixelRatio': 3}})
    self.assertTrue(driver.capabilities['mobileEmulationEnabled'])
    self.assertEqual(360, driver.ExecuteScript('return window.innerWidth'))
    self.assertEqual(640, driver.ExecuteScript('return window.innerHeight'))

  def testUserAgent(self):
    driver = self.CreateDriver(
        mobile_emulation = {'userAgent': 'Agent Smith'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    body_tag = driver.FindElement('tag name', 'body')
    self.assertEqual("Agent Smith", body_tag.GetText())

  def testDeviceName(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'Google Nexus 5'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual(360, driver.ExecuteScript('return window.innerWidth'))
    self.assertEqual(640, driver.ExecuteScript('return window.innerHeight'))
    body_tag = driver.FindElement('tag name', 'body')
    self.assertEqual(
        'Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5 Build/JOP40D) AppleW'
        'ebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166 Mobile Safari/53'
        '5.19',
        body_tag.GetText())


class ChromeDriverLogTest(unittest.TestCase):
  """Tests that chromedriver produces the expected log file."""

  UNEXPECTED_CHROMEOPTION_CAP = 'unexpected_chromeoption_capability'
  LOG_MESSAGE = 'unrecognized chrome option: %s' % UNEXPECTED_CHROMEOPTION_CAP

  def testChromeDriverLog(self):
    _, tmp_log_path = tempfile.mkstemp(prefix='chromedriver_log_')
    chromedriver_server = server.Server(
        _CHROMEDRIVER_BINARY, log_path=tmp_log_path)
    try:
      driver = chromedriver.ChromeDriver(
          chromedriver_server.GetUrl(), chrome_binary=_CHROME_BINARY,
          experimental_options={ self.UNEXPECTED_CHROMEOPTION_CAP : 1 })
      driver.Quit()
    except chromedriver.ChromeDriverException, e:
      self.assertTrue(self.LOG_MESSAGE in e.message)
    finally:
      chromedriver_server.Kill()
    with open(tmp_log_path, 'r') as f:
      self.assertTrue(self.LOG_MESSAGE in f.read())


class PerformanceLoggerTest(ChromeDriverBaseTest):
  """Tests chromedriver tracing support and Inspector event collection."""

  def testPerformanceLogger(self):
    driver = self.CreateDriver(
        experimental_options={'perfLoggingPrefs': {
            'enableTimeline': True,
            'traceCategories': 'webkit.console,blink.console'
          }}, performance_log_level='ALL')
    driver.Load(
        ChromeDriverTest._http_server.GetUrl() + '/chromedriver/empty.html')
    # Mark the timeline; later we will verify the marks appear in the trace.
    driver.ExecuteScript('console.time("foobar")')
    driver.ExecuteScript('console.timeEnd("foobar")')
    logs = driver.GetLog('performance')
    driver.Quit()

    marked_timeline_events = []
    seen_log_domains = {}
    for entry in logs:
      devtools_message = json.loads(entry['message'])['message']
      method = devtools_message['method']
      domain = method[:method.find('.')]
      seen_log_domains[domain] = True
      if method != 'Tracing.dataCollected':
        continue
      self.assertTrue('params' in devtools_message)
      self.assertTrue(isinstance(devtools_message['params'], dict))
      cat = devtools_message['params'].get('cat', '')
      # Depending on Chrome version, the events may occur for the webkit.console
      # or blink.console category. They will only occur for one of them.
      if (cat == 'blink.console' or cat == 'webkit.console'):
        self.assertTrue(devtools_message['params']['name'] == 'foobar')
        marked_timeline_events.append(devtools_message)
    self.assertEquals(2, len(marked_timeline_events))
    self.assertEquals({'Network', 'Page', 'Timeline', 'Tracing'},
                      set(seen_log_domains.keys()))


class SessionHandlingTest(ChromeDriverBaseTest):
  """Tests for session operations."""
  def testQuitASessionMoreThanOnce(self):
    driver = self.CreateDriver()
    driver.Quit()
    driver.Quit()


class RemoteBrowserTest(ChromeDriverBaseTest):
  """Tests for ChromeDriver remote browser capability."""
  def setUp(self):
    self.assertTrue(_CHROME_BINARY is not None,
                    'must supply a chrome binary arg')

  def testConnectToRemoteBrowser(self):
    port = self.FindFreePort()
    temp_dir = util.MakeTempDir()
    process = subprocess.Popen([_CHROME_BINARY,
                                '--remote-debugging-port=%d' % port,
                                '--user-data-dir=%s' % temp_dir])
    if process is None:
      raise RuntimeError('Chrome could not be started with debugging port')
    try:
      driver = self.CreateDriver(debugger_address='127.0.0.1:%d' % port)
      driver.ExecuteScript('console.info("%s")' % 'connecting at %d!' % port)
      driver.Quit()
    finally:
      process.terminate()

  def FindFreePort(self):
    for port in range(10000, 10100):
      try:
        socket.create_connection(('127.0.0.1', port), 0.2).close()
      except socket.error:
        return port
    raise RuntimeError('Cannot find open port')

class PerfTest(ChromeDriverBaseTest):
  """Tests for ChromeDriver perf."""
  def setUp(self):
    self.assertTrue(_REFERENCE_CHROMEDRIVER is not None,
                    'must supply a reference-chromedriver arg')

  def _RunDriverPerfTest(self, name, test_func):
    """Runs a perf test comparing a reference and new ChromeDriver server.

    Args:
      name: The name of the perf test.
      test_func: Called with the server url to perform the test action. Must
                 return the time elapsed.
    """
    class Results(object):
      ref = []
      new = []

    ref_server = server.Server(_REFERENCE_CHROMEDRIVER)
    results = Results()
    result_url_pairs = zip([results.new, results.ref],
                           [_CHROMEDRIVER_SERVER_URL, ref_server.GetUrl()])
    for iteration in range(30):
      for result, url in result_url_pairs:
        result += [test_func(url)]
      # Reverse the order for the next run.
      result_url_pairs = result_url_pairs[::-1]

    def PrintResult(build, result):
      mean = sum(result) / len(result)
      avg_dev = sum([abs(sample - mean) for sample in result]) / len(result)
      print 'perf result', build, name, mean, avg_dev, result
      util.AddBuildStepText('%s %s: %.3f+-%.3f' % (
          build, name, mean, avg_dev))

    # Discard first result, which may be off due to cold start.
    PrintResult('new', results.new[1:])
    PrintResult('ref', results.ref[1:])

  def testSessionStartTime(self):
    def Run(url):
      start = time.time()
      driver = self.CreateDriver(url)
      end = time.time()
      driver.Quit()
      return end - start
    self._RunDriverPerfTest('session start', Run)

  def testSessionStopTime(self):
    def Run(url):
      driver = self.CreateDriver(url)
      start = time.time()
      driver.Quit()
      end = time.time()
      return end - start
    self._RunDriverPerfTest('session stop', Run)

  def testColdExecuteScript(self):
    def Run(url):
      driver = self.CreateDriver(url)
      start = time.time()
      driver.ExecuteScript('return 1')
      end = time.time()
      driver.Quit()
      return end - start
    self._RunDriverPerfTest('cold exe js', Run)

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--chromedriver',
      help='Path to chromedriver server (REQUIRED!)')
  parser.add_option(
      '', '--log-path',
      help='Output verbose server logs to this file')
  parser.add_option(
      '', '--reference-chromedriver',
      help='Path to the reference chromedriver server')
  parser.add_option(
      '', '--chrome', help='Path to a build of the chrome binary')
  parser.add_option(
      '', '--chrome-version', default='HEAD',
      help='Version of chrome. Default is \'HEAD\'.')
  parser.add_option(
      '', '--filter', type='string', default='*',
      help=('Filter for specifying what tests to run, "*" will run all. E.g., '
            '*testStartStop'))
  parser.add_option(
      '', '--android-package',
      help=('Android package key. Possible values: ' +
            str(_ANDROID_NEGATIVE_FILTER.keys())))
  options, args = parser.parse_args()

  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if not options.chromedriver or not os.path.exists(options.chromedriver):
    parser.error('chromedriver is required or the given path is invalid.' +
                 'Please run "%s --help" for help' % __file__)

  global _CHROMEDRIVER_BINARY
  _CHROMEDRIVER_BINARY = options.chromedriver

  if (options.android_package and
      options.android_package not in _ANDROID_NEGATIVE_FILTER):
    parser.error('Invalid --android-package')

  chromedriver_server = server.Server(_CHROMEDRIVER_BINARY, options.log_path)
  global _CHROMEDRIVER_SERVER_URL
  _CHROMEDRIVER_SERVER_URL = chromedriver_server.GetUrl()

  global _REFERENCE_CHROMEDRIVER
  _REFERENCE_CHROMEDRIVER = util.GetAbsolutePathOfUserPath(
      options.reference_chromedriver)

  global _CHROME_BINARY
  if options.chrome:
    _CHROME_BINARY = util.GetAbsolutePathOfUserPath(options.chrome)
  else:
    _CHROME_BINARY = None

  global _ANDROID_PACKAGE_KEY
  _ANDROID_PACKAGE_KEY = options.android_package

  if options.filter == '*':
    if _ANDROID_PACKAGE_KEY:
      negative_filter = _ANDROID_NEGATIVE_FILTER[_ANDROID_PACKAGE_KEY]
    else:
      negative_filter = _GetDesktopNegativeFilter(options.chrome_version)
    options.filter = '*-' + ':__main__.'.join([''] + negative_filter)

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  tests = unittest_util.FilterTestSuite(all_tests_suite, options.filter)
  ChromeDriverTest.GlobalSetUp()
  MobileEmulationCapabilityTest.GlobalSetUp()
  result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(tests)
  ChromeDriverTest.GlobalTearDown()
  MobileEmulationCapabilityTest.GlobalTearDown()
  sys.exit(len(result.failures) + len(result.errors))
