# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extended WebDriver interface that uses helper extension.

This file is makeshift and should eventually be switched over to
using the new ChromeDriver python interface. However, as that is
not quite ready, this class simply installs a helper extension
and executes scripts in the background page to access extension
APIs.

This may end up being merged with chrome/test/ext_auto, if they
accomplish similar enough purposes. For now, integration with that
is a bit premature.
"""

import os

from selenium import webdriver


_CHROME_GET_VIEW_HANDLES = 'chrome.getViewHandles'
_EXTENSION = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'ext_auto')


class Chrome(webdriver.Remote):
  """Extended WebDriver interface that uses helper extension."""

  def __init__(self, url, desired_capabilities, options=None):
    """Initializes Chrome object.

    If both desired_capabilities and options have the same settings, the
    settings from options will be used.

    Args:
      url: The URL of the ChromeDriver Service.
      desired_capabilities: Chrome capabilities dictionary.
      options: chrome_options.ChromeOptions object. Settings in options will
          overwrite settings in desired_capabilities.

    Raises:
      RuntimeError: Unable to find helper extension.
    """
    if options is not None:
      desired_capabilities.update(options.GetCapabilities())
    switches = desired_capabilities.get('chrome.switches', [])
    switches += ['--load-extension=' + _EXTENSION]
    desired_capabilities['chrome.switches'] = switches
    super(Chrome, self).__init__(url, desired_capabilities)

    custom_commands = {
        _CHROME_GET_VIEW_HANDLES:
            ('GET', '/session/$sessionId/chrome/views'),
    }
    self.command_executor._commands.update(custom_commands)
    views = self.execute(_CHROME_GET_VIEW_HANDLES)['value']
    self.set_script_timeout(30)  # TODO(kkania): Make this configurable.
    for view in views:
      if view.get('extension_id') == 'aapnijgdinlhnhlmodcfapnahmbfebeb':
        self._extension = view['handle']
        break
    else:
      raise RuntimeError('Unable to find helper extension')

  def _execute_extension_command(self, name, params={}):
    """Executes an extension command.

    When Chrome is started, a helper extension is loaded which provides
    a simple synchronous API for manipulating Chrome via the extension
    APIs. Communication with the extension is accomplished by executing
    a script in the background page of the extension which calls the
    'executeCommand' function with the name of the command, a parameter
    dictionary, and a callback function that can be used to signal
    when the command is finished and potentially send a return value.
    """
    old_window = self.current_window_handle
    self.switch_to_window(self._extension)
    self.execute_async_script(
        'executeCommand.apply(null, arguments)', name, params)
    self.switch_to_window(old_window)

  def create_tab(self, url=None):
    """Creates a new tab with the given URL and switches to it.

    If no URL is provided, the homepage will be used.
    """
    params = {}
    if url is not None:
      params['url'] = url
    self._execute_extension_command('createTab', params)
    self.switch_to_window(self.window_handles[-1])

  def create_blank_tab(self):
    """Creates a new blank tab and switches to it."""
    self.create_tab('about:blank')
