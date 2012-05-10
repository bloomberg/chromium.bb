# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome WebDriver that implements extra Chrome-specific functionality.

This module is experimental and will change and break without warning.
Use at your own risk.

Style Note: Because this is an extension to the WebDriver python API and
since this module will eventually be moved into the webdriver codebase, the
code follows WebDriver naming conventions for functions.
"""

from selenium.common.exceptions import WebDriverException
from selenium.webdriver.remote.webdriver import WebDriver as RemoteWebDriver


class _ViewType(object):
  """Constants representing different web view types in Chrome."""
  TAB = 1
  EXTENSION_POPUP = 2
  EXTENSION_BG_PAGE = 3
  EXTENSION_INFOBAR = 4


class WebDriver(RemoteWebDriver):
  """
  Controls Chrome and provides additional Chrome-specific functionality not in
  the WebDriver standard.

  This class is experimental and subject to change and break without warning.
  Use at your own risk.
  """

  _CHROME_GET_EXTENSIONS = "chrome.getExtensions"
  _CHROME_INSTALL_EXTENSION = "chrome.installExtension"
  _CHROME_GET_EXTENSION_INFO = "chrome.getExtensionInfo"
  _CHROME_MODIFY_EXTENSION = "chrome.setExtensionState"
  _CHROME_UNINSTALL_EXTENSION = "chrome.uninstallExtension"
  _CHROME_GET_VIEW_HANDLES = "chrome.getViewHandles"
  _CHROME_DUMP_HEAP_PROFILE = "chrome.dumpHeapProfile"

  def __init__(self, url, desired_capabilities={}):
    """Creates a WebDriver that controls Chrome via ChromeDriver.

    Args:
      url: The URL of a running ChromeDriver server.
      desired_capabilities: Requested capabilities for the new WebDriver
          session.
    """
    RemoteWebDriver.__init__(self,
        command_executor=url,
        desired_capabilities=desired_capabilities)

    # Add custom commands.
    custom_commands = {
    WebDriver._CHROME_GET_EXTENSIONS:
        ('GET', '/session/$sessionId/chrome/extensions'),
    WebDriver._CHROME_INSTALL_EXTENSION:
        ('POST', '/session/$sessionId/chrome/extensions'),
    WebDriver._CHROME_GET_EXTENSION_INFO:
        ('GET', '/session/$sessionId/chrome/extension/$id'),
    WebDriver._CHROME_MODIFY_EXTENSION:
        ('POST', '/session/$sessionId/chrome/extension/$id'),
    WebDriver._CHROME_UNINSTALL_EXTENSION:
        ('DELETE', '/session/$sessionId/chrome/extension/$id'),
    WebDriver._CHROME_GET_VIEW_HANDLES:
        ('GET', '/session/$sessionId/chrome/views'),
    WebDriver._CHROME_DUMP_HEAP_PROFILE:
        ('POST', '/session/$sessionId/chrome/heapprofilerdump')
    }
    self.command_executor._commands.update(custom_commands)

  def get_installed_extensions(self):
    """Returns a list of installed extensions."""
    ids = RemoteWebDriver.execute(
        self, WebDriver._CHROME_GET_EXTENSIONS)['value']
    return map(lambda id: Extension(self, id), ids)

  def install_extension(self, path):
    """Install the extension at the given path.

    Args:
      path: Path to packed or unpacked extension to install.

    Returns:
      The installed extension.
    """
    params = {'path': path}
    id = RemoteWebDriver.execute(
        self, WebDriver._CHROME_INSTALL_EXTENSION, params)['value']
    return Extension(self, id)

  def dump_heap_profile(self, reason):
    """Dumps a heap profile.  It works only on Linux and ChromeOS.

    We need an environment variable "HEAPPROFILE" set to a directory and a
    filename prefix, for example, "/tmp/prof".  In a case of this example,
    heap profiles will be dumped into "/tmp/prof.(pid).0002.heap",
    "/tmp/prof.(pid).0003.heap", and so on.  Nothing happens when this
    function is called without the env.

    Args:
      reason: A string which describes the reason for dumping a heap profile.
              The reason will be included in the logged message.
              Examples:
                'To check memory leaking'
                'For WebDriver tests'
    """
    if self.IsLinux():  # IsLinux() also implies IsChromeOS().
      params = {'reason': reason}
      RemoteWebDriver.execute(self, WebDriver._CHROME_DUMP_HEAP_PROFILE, params)
    else:
      raise WebDriverException('Heap-profiling is not supported in this OS.')


class Extension(object):
  """Represents a Chrome extension/app."""

  def __init__(self, parent, id):
    self._parent = parent
    self._id = id

  @property
  def id(self):
    return self._id

  def get_name(self):
    return self._get_info()['name']

  def get_version(self):
    return self._get_info()['version']

  def is_enabled(self):
    return self._get_info()['is_enabled']

  def set_enabled(self, value):
    self._execute(WebDriver._CHROME_MODIFY_EXTENSION, {'enable': value})

  def is_page_action_visible(self):
    """Returns whether the page action is visible in the currently targeted tab.

    This will fail if the current target is not a tab.
    """
    return self._get_info()['is_page_action_visible']

  def uninstall(self):
    self._execute(WebDriver._CHROME_UNINSTALL_EXTENSION)

  def click_browser_action(self):
    """Clicks the browser action in the currently targeted tab.

    This will fail if the current target is not a tab.
    """
    self._execute(WebDriver._CHROME_MODIFY_EXTENSION,
                  {'click_button': 'browser_action'})

  def click_page_action(self):
    """Clicks the page action in the currently targeted tab.

    This will fail if the current target is not a tab.
    """
    self._execute(WebDriver._CHROME_MODIFY_EXTENSION,
                  {'click_button': 'page_action'})

  def get_bg_page_handle(self):
    """Returns the window handle for the background page.

    This handle can be used with |WebDriver.switch_to_window|.

    Returns:
      The window handle, or None if there is no background page.
    """
    bg_pages = filter(lambda view: view['type'] == _ViewType.EXTENSION_BG_PAGE,
                      self._get_views())
    if len(bg_pages) > 0:
      return bg_pages[0]['handle']
    return None

  def get_popup_handle(self):
    """Returns the window handle for the open browser/page action popup.

    This handle can be used with |WebDriver.switch_to_window|.

    Returns:
      The window handle, or None if there is no popup open.
    """
    popups = filter(lambda view: view['type'] == _ViewType.EXTENSION_POPUP,
                    self._get_views())
    if len(popups) > 0:
      return popups[0]['handle']
    return None

  def get_infobar_handles(self):
    """Returns a list of window handles for all open infobars of this extension.

    This handle can be used with |WebDriver.switch_to_window|.
    """
    infobars = filter(lambda view: view['type'] == _ViewType.EXTENSION_INFOBAR,
                      self._get_views())
    return map(lambda view: view['handle'], infobars)

  def _get_info(self):
    """Returns a dictionary of all this extension's info."""
    return self._execute(WebDriver._CHROME_GET_EXTENSION_INFO)['value']

  def _get_views(self):
    """Returns a list of view information for this extension."""
    views = self._parent.execute(WebDriver._CHROME_GET_VIEW_HANDLES)['value']
    ext_views = []
    for view in views:
      if 'extension_id' in view and view['extension_id'] == self._id:
        ext_views += [view]
    return ext_views

  def _execute(self, command, params=None):
    """Executes a command against the underlying extension.

    Args:
      command: The name of the command to execute.
      params: A dictionary of named parameters to send with the command.

    Returns:
      The command's JSON response loaded into a dictionary object.
    """
    if not params:
        params = {}
    params['id'] = self._id
    return self._parent.execute(command, params)
