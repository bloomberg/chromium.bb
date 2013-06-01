# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import os
import socket
import subprocess
import time
import urllib2

import command_executor
from command_executor import Command
from webelement import WebElement


class ChromeDriverException(Exception):
  pass
class NoSuchElement(ChromeDriverException):
  pass
class NoSuchFrame(ChromeDriverException):
  pass
class UnknownCommand(ChromeDriverException):
  pass
class StaleElementReference(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class JavaScriptError(ChromeDriverException):
  pass
class XPathLookupError(ChromeDriverException):
  pass
class NoSuchWindow(ChromeDriverException):
  pass
class InvalidCookieDomain(ChromeDriverException):
  pass
class ScriptTimeout(ChromeDriverException):
  pass
class InvalidSelector(ChromeDriverException):
  pass
class SessionNotCreatedException(ChromeDriverException):
  pass
class NoSuchSession(ChromeDriverException):
  pass

def _ExceptionForResponse(response):
  exception_class_map = {
    7: NoSuchElement,
    8: NoSuchFrame,
    9: UnknownCommand,
    10: StaleElementReference,
    13: UnknownError,
    17: JavaScriptError,
    19: XPathLookupError,
    23: NoSuchWindow,
    24: InvalidCookieDomain,
    28: ScriptTimeout,
    32: InvalidSelector,
    33: SessionNotCreatedException,
    100: NoSuchSession
  }
  status = response['status']
  msg = response['value']['message']
  return exception_class_map.get(status, ChromeDriverException)(msg)


class ChromeDriver(object):
  """Starts and controls a single Chrome instance on this machine."""

  def __init__(self, server_url, chrome_binary=None, android_package=None,
               chrome_switches=None, chrome_extensions=None,
               chrome_log_path=None):
    self._executor = command_executor.CommandExecutor(server_url)

    options = {}
    if android_package:
      options['androidPackage'] = android_package
    elif chrome_binary:
      options['binary'] = chrome_binary

    if chrome_switches:
      assert type(chrome_switches) is list
      options['args'] = chrome_switches

    if chrome_extensions:
      assert type(chrome_extensions) is list
      options['extensions'] = chrome_extensions

    if chrome_log_path:
      assert type(chrome_log_path) is str
      options['logPath'] = chrome_log_path

    params = {
      'desiredCapabilities': {
        'chromeOptions': options
      }
    }

    self._session_id = self._executor.Execute(
        Command.NEW_SESSION, params)['sessionId']

  def _WrapValue(self, value):
    """Wrap value from client side for chromedriver side."""
    if isinstance(value, dict):
      converted = {}
      for key, val in value.items():
        converted[key] = self._WrapValue(val)
      return converted
    elif isinstance(value, WebElement):
      return {'ELEMENT': value._id}
    elif isinstance(value, list):
      return list(self._WrapValue(item) for item in value)
    else:
      return value

  def _UnwrapValue(self, value):
    """Unwrap value from chromedriver side for client side."""
    if isinstance(value, dict):
      if (len(value) == 1 and 'ELEMENT' in value
          and isinstance(value['ELEMENT'], basestring)):
        return WebElement(self, value['ELEMENT'])
      else:
        unwraped = {}
        for key, val in value.items():
          unwraped[key] = self._UnwrapValue(val)
        return unwraped
    elif isinstance(value, list):
      return list(self._UnwrapValue(item) for item in value)
    else:
      return value

  def ExecuteCommand(self, command, params={}):
    params['sessionId'] = self._session_id
    params = self._WrapValue(params)
    response = self._executor.Execute(command, params)
    if response['status'] != 0:
      raise _ExceptionForResponse(response)
    return self._UnwrapValue(response['value'])

  def GetWindowHandles(self):
    return self.ExecuteCommand(Command.GET_WINDOW_HANDLES)

  def SwitchToWindow(self, handle_or_name):
    self.ExecuteCommand(Command.SWITCH_TO_WINDOW, {'name': handle_or_name})

  def GetCurrentWindowHandle(self):
    return self.ExecuteCommand(Command.GET_CURRENT_WINDOW_HANDLE)

  def CloseWindow(self):
    self.ExecuteCommand(Command.CLOSE)

  def Load(self, url):
    self.ExecuteCommand(Command.GET, {'url': url})

  def ExecuteScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteCommand(
        Command.EXECUTE_SCRIPT, {'script': script, 'args': converted_args})

  def ExecuteAsyncScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteCommand(
        Command.EXECUTE_ASYNC_SCRIPT,
        {'script': script, 'args': converted_args})

  def SwitchToFrame(self, id_or_name):
    self.ExecuteCommand(Command.SWITCH_TO_FRAME, {'id': id_or_name})

  def SwitchToFrameByIndex(self, index):
    self.SwitchToFrame(index)

  def SwitchToMainFrame(self):
    self.SwitchToFrame(None)

  def GetTitle(self):
    return self.ExecuteCommand(Command.GET_TITLE)

  def GetPageSource(self):
    return self.ExecuteCommand(Command.GET_PAGE_SOURCE)

  def FindElement(self, strategy, target):
    return self.ExecuteCommand(
        Command.FIND_ELEMENT, {'using': strategy, 'value': target})

  def FindElements(self, strategy, target):
    return self.ExecuteCommand(
        Command.FIND_ELEMENTS, {'using': strategy, 'value': target})

  def SetTimeout(self, type, timeout):
    return self.ExecuteCommand(
        Command.SET_TIMEOUT, {'type' : type, 'ms': timeout})

  def GetCurrentUrl(self):
    return self.ExecuteCommand(Command.GET_CURRENT_URL)

  def GoBack(self):
    return self.ExecuteCommand(Command.GO_BACK)

  def GoForward(self):
    return self.ExecuteCommand(Command.GO_FORWARD)

  def Refresh(self):
    return self.ExecuteCommand(Command.REFRESH)

  def MouseMoveTo(self, element=None, x_offset=None, y_offset=None):
    params = {}
    if element is not None:
      params['element'] = element._id
    if x_offset is not None:
      params['xoffset'] = x_offset
    if y_offset is not None:
      params['yoffset'] = y_offset
    self.ExecuteCommand(Command.MOUSE_MOVE_TO, params)

  def MouseClick(self, button=0):
    self.ExecuteCommand(Command.MOUSE_CLICK, {'button': button})

  def MouseButtonDown(self, button=0):
    self.ExecuteCommand(Command.MOUSE_BUTTON_DOWN, {'button': button})

  def MouseButtonUp(self, button=0):
    self.ExecuteCommand(Command.MOUSE_BUTTON_UP, {'button': button})

  def MouseDoubleClick(self, button=0):
    self.ExecuteCommand(Command.MOUSE_DOUBLE_CLICK, {'button': button})

  def GetCookies(self):
    return self.ExecuteCommand(Command.GET_COOKIES)

  def AddCookie(self, cookie):
    self.ExecuteCommand(Command.ADD_COOKIE, {'cookie': cookie})

  def DeleteCookie(self, name):
    self.ExecuteCommand(Command.DELETE_COOKIE, {'name': name})

  def DeleteAllCookies(self):
    self.ExecuteCommand(Command.DELETE_ALL_COOKIES)

  def IsAlertOpen(self):
    return self.ExecuteCommand(Command.GET_ALERT)

  def GetAlertMessage(self):
    return self.ExecuteCommand(Command.GET_ALERT_TEXT)

  def HandleAlert(self, accept, prompt_text=''):
    if prompt_text:
      self.ExecuteCommand(Command.SET_ALERT_VALUE, {'text': prompt_text})
    if accept:
      cmd = Command.ACCEPT_ALERT
    else:
      cmd = Command.DISMISS_ALERT
    self.ExecuteCommand(cmd)

  def IsLoading(self):
    return self.ExecuteCommand(Command.IS_LOADING)

  def GetWindowPosition(self):
    position = self.ExecuteCommand(Command.GET_WINDOW_POSITION,
                                   {'windowHandle': 'current'})
    return [position['x'], position['y']]

  def SetWindowPosition(self, x, y):
    self.ExecuteCommand(Command.SET_WINDOW_POSITION,
                        {'windowHandle': 'current', 'x': x, 'y': y})

  def GetWindowSize(self):
    size = self.ExecuteCommand(Command.GET_WINDOW_SIZE,
                               {'windowHandle': 'current'})
    return [size['width'], size['height']]

  def SetWindowSize(self, width, height):
    self.ExecuteCommand(
        Command.SET_WINDOW_SIZE,
        {'windowHandle': 'current', 'width': width, 'height': height})

  def MaximizeWindow(self):
    self.ExecuteCommand(Command.MAXIMIZE_WINDOW, {'windowHandle': 'current'})

  def Quit(self):
    """Quits the browser and ends the session."""
    self.ExecuteCommand(Command.QUIT)


class Server(object):
  """A running ChromeDriver server."""

  def __init__(self, exe_path):
    """Starts the ChromeDriver server and waits for it to be ready.

    Args:
      exe_path: path to the ChromeDriver executable
    Raises:
      RuntimeError if ChromeDriver fails to start
    """
    if not os.path.exists(exe_path):
      raise RuntimeError('ChromeDriver exe not found at: ' + exe_path)

    port = self._FindOpenPort()
    chromedriver_args = [exe_path, '--port=%d' % port]
    self._process = subprocess.Popen(chromedriver_args)
    self._url = 'http://127.0.0.1:%d' % port
    if self._process is None:
      raise RuntimeError('ChromeDriver server cannot be started')

    max_time = time.time() + 10
    while not self.IsRunning():
      if time.time() > max_time:
        self._process.terminate()
        raise RuntimeError('ChromeDriver server did not start')
      time.sleep(0.1)

    atexit.register(self.Kill)

  def _FindOpenPort(self):
    for port in range(9500, 10000):
      try:
        socket.create_connection(('127.0.0.1', port), 0.2).close()
      except socket.error:
        return port
    raise RuntimeError('Cannot find open port to launch ChromeDriver')

  def GetUrl(self):
    return self._url

  def IsRunning(self):
    """Returns whether the server is up and running."""
    try:
      urllib2.urlopen(self.GetUrl() + '/status')
      return True
    except urllib2.URLError:
      return False

  def Kill(self):
    """Kills the ChromeDriver server, if it is running."""
    if self._process is None:
      return

    try:
      urllib2.urlopen(self.GetUrl() + '/shutdown', timeout=10).close()
    except:
      self._process.terminate()
    self._process.wait()
    self._process = None
