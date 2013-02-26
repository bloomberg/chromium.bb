# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import json

from webelement import WebElement

class ChromeDriverException(Exception):
  pass
class NoSuchElement(ChromeDriverException):
  pass
class UnknownCommand(ChromeDriverException):
  pass
class StaleElementReference(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class XPathLookupError(ChromeDriverException):
  pass
class NoSuchWindow(ChromeDriverException):
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
    9: UnknownCommand,
    10: StaleElementReference,
    13: UnknownError,
    19: XPathLookupError,
    23: NoSuchWindow,
    32: InvalidSelector,
    33: SessionNotCreatedException,
    100: NoSuchSession
  }
  status = response['status']
  msg = response['value']['message']
  return exception_class_map.get(status, ChromeDriverException)(msg)

class ChromeDriver(object):
  """Starts and controls a single Chrome instance on this machine."""

  def __init__(self, lib_path, chrome_binary=None, android_package=None,
               chrome_switches=None):
    self._lib = ctypes.CDLL(lib_path)

    options = {}
    if android_package:
      options['android_package'] = android_package
    elif chrome_binary:
      options['binary'] = chrome_binary

    if chrome_switches:
      assert type(chrome_switches) is list
      options['args'] = chrome_switches

    if options:
      params = {
        'desiredCapabilities': {
          'chromeOptions': options
        }
      }
    else:
      params = {}

    self._session_id = self._ExecuteCommand('newSession', params)['sessionId']

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

  def _ExecuteCommand(self, name, params={}, session_id=''):
    cmd = {
      'name': name,
      'parameters': params,
      'sessionId': session_id
    }
    cmd_json = json.dumps(cmd)
    response_data = ctypes.c_char_p()
    response_size = ctypes.c_uint()
    self._lib.ExecuteCommand(
        ctypes.c_char_p(cmd_json),
        ctypes.c_uint(len(cmd_json)),
        ctypes.byref(response_data),
        ctypes.byref(response_size))
    response_json = ctypes.string_at(response_data, response_size.value)
    self._lib.Free(response_data)
    response = json.loads(response_json)
    if response['status'] != 0:
      raise _ExceptionForResponse(response)
    return response

  def ExecuteSessionCommand(self, name, params={}):
    params = self._WrapValue(params)
    return self._UnwrapValue(
        self._ExecuteCommand(name, params, self._session_id)['value'])

  def GetWindowHandles(self):
    return self.ExecuteSessionCommand('getWindowHandles')

  def SwitchToWindow(self, handle_or_name):
    self.ExecuteSessionCommand('switchToWindow', {'name': handle_or_name})

  def GetCurrentWindowHandle(self):
    return self.ExecuteSessionCommand('getCurrentWindowHandle')

  def CloseWindow(self):
    self.ExecuteSessionCommand('close')

  def Load(self, url):
    self.ExecuteSessionCommand('get', {'url': url})

  def ExecuteScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteSessionCommand(
        'executeScript', {'script': script, 'args': converted_args})

  def SwitchToFrame(self, id_or_name):
    self.ExecuteSessionCommand('switchToFrame', {'id': id_or_name})

  def SwitchToFrameByIndex(self, index):
    self.SwitchToFrame(index)

  def SwitchToMainFrame(self):
    self.SwitchToFrame(None)

  def GetTitle(self):
    return self.ExecuteSessionCommand('getTitle')

  def GetPageSource(self):
    return self.ExecuteSessionCommand('getPageSource')

  def FindElement(self, strategy, target):
    return self.ExecuteSessionCommand(
        'findElement', {'using': strategy, 'value': target})

  def FindElements(self, strategy, target):
    return self.ExecuteSessionCommand(
        'findElements', {'using': strategy, 'value': target})

  def SetTimeout(self, type, timeout):
    return self.ExecuteSessionCommand(
        'setTimeout', {'type' : type, 'ms': timeout})

  def GetCurrentUrl(self):
    return self.ExecuteSessionCommand('getCurrentUrl')

  def GoBack(self):
    return self.ExecuteSessionCommand('goBack')

  def GoForward(self):
    return self.ExecuteSessionCommand('goForward')

  def Refresh(self):
    return self.ExecuteSessionCommand('refresh')

  def MouseMoveTo(self, element=None, x_offset=None, y_offset=None):
    params = {}
    if element is not None:
      params['element'] = element._id
    if x_offset is not None:
      params['xoffset'] = x_offset
    if y_offset is not None:
      params['yoffset'] = y_offset
    self.ExecuteSessionCommand('mouseMoveTo', params)

  def MouseClick(self, button=0):
    self.ExecuteSessionCommand('mouseClick', {'button': button})

  def MouseButtonDown(self, button=0):
    self.ExecuteSessionCommand('mouseButtonDown', {'button': button})

  def MouseButtonUp(self, button=0):
    self.ExecuteSessionCommand('mouseButtonUp', {'button': button})

  def MouseDoubleClick(self, button=0):
    self.ExecuteSessionCommand('mouseDoubleClick', {'button': button})

  def IsAlertOpen(self):
    return self.ExecuteSessionCommand('getAlert')

  def GetAlertMessage(self):
    return self.ExecuteSessionCommand('getAlertText')

  def HandleAlert(self, accept, prompt_text=''):
    if prompt_text:
      self.ExecuteSessionCommand('setAlertValue', {'text': prompt_text})
    if accept:
      cmd = 'acceptAlert'
    else:
      cmd = 'dismissAlert'
    self.ExecuteSessionCommand(cmd)

  def Quit(self):
    """Quits the browser and ends the session."""
    self.ExecuteSessionCommand('quit')
