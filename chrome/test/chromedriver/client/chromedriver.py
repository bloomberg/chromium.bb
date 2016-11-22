# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import platform

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
class ElementNotVisible(ChromeDriverException):
  pass
class InvalidElementState(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class JavaScriptError(ChromeDriverException):
  pass
class XPathLookupError(ChromeDriverException):
  pass
class Timeout(ChromeDriverException):
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
class UnexpectedAlertOpen(ChromeDriverException):
  pass
class NoAlertOpen(ChromeDriverException):
  pass

def _ExceptionForLegacyResponse(response):
  exception_class_map = {
    6: NoSuchSession,
    7: NoSuchElement,
    8: NoSuchFrame,
    9: UnknownCommand,
    10: StaleElementReference,
    11: ElementNotVisible,
    12: InvalidElementState,
    13: UnknownError,
    17: JavaScriptError,
    19: XPathLookupError,
    21: Timeout,
    23: NoSuchWindow,
    24: InvalidCookieDomain,
    26: UnexpectedAlertOpen,
    27: NoAlertOpen,
    28: ScriptTimeout,
    32: InvalidSelector,
    33: SessionNotCreatedException
  }
  status = response['status']
  msg = response['value']['message']
  return exception_class_map.get(status, ChromeDriverException)(msg)

def _ExceptionForStandardResponse(response):
  exception_map = {
    'no such session' : NoSuchSession,
    'no such element': NoSuchElement,
    'no such frame': NoSuchFrame,
    'unknown command': UnknownCommand,
    'stale element reference': StaleElementReference,
    'element not visible': ElementNotVisible,
    'invalid element state': InvalidElementState,
    'unknown error': UnknownError,
    'javascript error': JavaScriptError,
    'xpath lookup error': XPathLookupError,
    'timeout': Timeout,
    'no such window': NoSuchWindow,
    'invalid cookie domain': InvalidCookieDomain,
    'unexpected alert open': UnexpectedAlertOpen,
    'no alert open': NoAlertOpen,
    'asynchronous script timeout': ScriptTimeout,
    'invalid selector': InvalidSelector,
    'session not created exception': SessionNotCreatedException
  }

  error = response['error']
  msg = response['message']
  return exception_map.get(error, ChromeDriverException)(msg)

class ChromeDriver(object):
  """Starts and controls a single Chrome instance on this machine."""

  def __init__(self, server_url, chrome_binary=None, android_package=None,
               android_activity=None, android_process=None,
               android_use_running_app=None, chrome_switches=None,
               chrome_extensions=None, chrome_log_path=None,
               debugger_address=None, logging_prefs=None,
               mobile_emulation=None, experimental_options=None,
               download_dir=None, network_connection=None,
               send_w3c_capability=None, send_w3c_request=None,
               page_load_strategy=None):
    self._executor = command_executor.CommandExecutor(server_url)

    options = {}

    if experimental_options:
      assert isinstance(experimental_options, dict)
      options = experimental_options.copy()

    if android_package:
      options['androidPackage'] = android_package
      if android_activity:
        options['androidActivity'] = android_activity
      if android_process:
        options['androidProcess'] = android_process
      if android_use_running_app:
        options['androidUseRunningApp'] = android_use_running_app
    elif chrome_binary:
      options['binary'] = chrome_binary

    # TODO(samuong): speculative fix for crbug.com/611886
    if (sys.platform.startswith('linux') and
        platform.architecture()[0] == '32bit'):
      if chrome_switches is None:
        chrome_switches = []
      chrome_switches.append('no-sandbox')

    if chrome_switches:
      assert type(chrome_switches) is list
      options['args'] = chrome_switches

    if mobile_emulation:
      assert type(mobile_emulation) is dict
      options['mobileEmulation'] = mobile_emulation

    if chrome_extensions:
      assert type(chrome_extensions) is list
      options['extensions'] = chrome_extensions

    if chrome_log_path:
      assert type(chrome_log_path) is str
      options['logPath'] = chrome_log_path

    if debugger_address:
      assert type(debugger_address) is str
      options['debuggerAddress'] = debugger_address

    if logging_prefs:
      assert type(logging_prefs) is dict
      log_types = ['client', 'driver', 'browser', 'server', 'performance']
      log_levels = ['ALL', 'DEBUG', 'INFO', 'WARNING', 'SEVERE', 'OFF']
      for log_type, log_level in logging_prefs.iteritems():
        assert log_type in log_types
        assert log_level in log_levels
    else:
      logging_prefs = {}

    download_prefs = {}
    if download_dir:
      if 'prefs' not in options:
        options['prefs'] = {}
      if 'download' not in options['prefs']:
        options['prefs']['download'] = {}
      options['prefs']['download']['default_directory'] = download_dir

    if send_w3c_capability:
      options['w3c'] = send_w3c_capability

    params = {
      'desiredCapabilities': {
        'chromeOptions': options,
        'loggingPrefs': logging_prefs
      }
    }

    if page_load_strategy:
      assert type(page_load_strategy) is str
      params['desiredCapabilities']['pageLoadStrategy'] = page_load_strategy

    if network_connection:
      params['desiredCapabilities']['networkConnectionEnabled'] = (
          network_connection)

    if send_w3c_request:
      params = {'capabilities': params}

    response = self._ExecuteCommand(Command.NEW_SESSION, params)
    if isinstance(response['status'], basestring):
      self.w3c_compliant = True
    elif isinstance(response['status'], int):
      self.w3c_compliant = False
    else:
      raise UnknownError("unexpected response")

    self._session_id = response['sessionId']
    self.capabilities = self._UnwrapValue(response['value'])

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

  def _ExecuteCommand(self, command, params={}):
    params = self._WrapValue(params)
    response = self._executor.Execute(command, params)
    if ('status' in response and isinstance(response['status'], int) and
        response['status'] != 0):
      raise _ExceptionForLegacyResponse(response)
    elif 'error' in response:
      raise _ExceptionForStandardResponse(response)
    return response

  def ExecuteCommand(self, command, params={}):
    params['sessionId'] = self._session_id
    response = self._ExecuteCommand(command, params)
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

  def LaunchApp(self, app_id):
    self.ExecuteCommand(Command.LAUNCH_APP, {'id': app_id})

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

  def SwitchToParentFrame(self):
    self.ExecuteCommand(Command.SWITCH_TO_PARENT_FRAME)

  def GetSessions(self):
    return self.ExecuteCommand(Command.GET_SESSIONS)

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

  def TouchDown(self, x, y):
    self.ExecuteCommand(Command.TOUCH_DOWN, {'x': x, 'y': y})

  def TouchUp(self, x, y):
    self.ExecuteCommand(Command.TOUCH_UP, {'x': x, 'y': y})

  def TouchMove(self, x, y):
    self.ExecuteCommand(Command.TOUCH_MOVE, {'x': x, 'y': y})

  def TouchScroll(self, element, xoffset, yoffset):
    params = {'element': element._id, 'xoffset': xoffset, 'yoffset': yoffset}
    self.ExecuteCommand(Command.TOUCH_SCROLL, params)

  def TouchFlick(self, element, xoffset, yoffset, speed):
    params = {
        'element': element._id,
        'xoffset': xoffset,
        'yoffset': yoffset,
        'speed': speed
    }
    self.ExecuteCommand(Command.TOUCH_FLICK, params)

  def TouchPinch(self, x, y, scale):
    params = {'x': x, 'y': y, 'scale': scale}
    self.ExecuteCommand(Command.TOUCH_PINCH, params)

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

  def GetLog(self, type):
    return self.ExecuteCommand(Command.GET_LOG, {'type': type})

  def GetAvailableLogTypes(self):
    return self.ExecuteCommand(Command.GET_AVAILABLE_LOG_TYPES)

  def IsAutoReporting(self):
    return self.ExecuteCommand(Command.IS_AUTO_REPORTING)

  def SetAutoReporting(self, enabled):
    self.ExecuteCommand(Command.SET_AUTO_REPORTING, {'enabled': enabled})

  def SetNetworkConditions(self, latency, download_throughput,
                           upload_throughput, offline=False):
    # Until http://crbug.com/456324 is resolved, we'll always set 'offline' to
    # False, as going "offline" will sever Chromedriver's connection to Chrome.
    params = {
        'network_conditions': {
            'offline': offline,
            'latency': latency,
            'download_throughput': download_throughput,
            'upload_throughput': upload_throughput
        }
    }
    self.ExecuteCommand(Command.SET_NETWORK_CONDITIONS, params)

  def SetNetworkConditionsName(self, network_name):
    self.ExecuteCommand(
        Command.SET_NETWORK_CONDITIONS, {'network_name': network_name})

  def GetNetworkConditions(self):
    conditions = self.ExecuteCommand(Command.GET_NETWORK_CONDITIONS)
    return {
        'latency': conditions['latency'],
        'download_throughput': conditions['download_throughput'],
        'upload_throughput': conditions['upload_throughput'],
        'offline': conditions['offline']
    }

  def GetNetworkConnection(self):
    return self.ExecuteCommand(Command.GET_NETWORK_CONNECTION)

  def DeleteNetworkConditions(self):
    self.ExecuteCommand(Command.DELETE_NETWORK_CONDITIONS)

  def SetNetworkConnection(self, connection_type):
    params = {'parameters': {'type': connection_type}}
    return self.ExecuteCommand(Command.SET_NETWORK_CONNECTION, params)

  def GetScreenOrientation(self):
    screen_orientation = self.ExecuteCommand(Command.GET_SCREEN_ORIENTATION)
    return {
       'orientation': screen_orientation['orientation']
    }

  def SetScreenOrientation(self, orientation_type):
    params = {'parameters': {'orientation': orientation_type}}
    self.ExecuteCommand(Command.SET_SCREEN_ORIENTATION, params)

  def DeleteScreenOrientationLock(self):
    self.ExecuteCommand(Command.DELETE_SCREEN_ORIENTATION)

  def SendKeys(self, *values):
    typing = []
    for value in values:
      if isinstance(value, int):
        value = str(value)
      for i in range(len(value)):
        typing.append(value[i])
    self.ExecuteCommand(Command.SEND_KEYS_TO_ACTIVE_ELEMENT, {'value': typing})
