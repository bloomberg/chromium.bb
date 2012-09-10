# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base GTalk tests.

This module contains a set of common utilities for querying
and manipulating the Google Talk Chrome Extension.
"""

import logging
import re
import os

import pyauto_gtalk
import pyauto
import pyauto_errors


class GTalkBaseTest(pyauto.PyUITest):
  """Base test class for testing GTalk."""

  _injected_js = None

  def Prompt(self, text):
    """Pause execution with debug output.

    Args:
      text: The debug output.
    """
    text = str(text)
    raw_input('--------------------> ' + text)

  def InstallGTalkExtension(self, gtalk_version):
    """Download and install the GTalk extension."""
    extension_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'gtalk',
                     gtalk_version + '.crx'))
    self.assertTrue(
        os.path.exists(extension_path),
        msg='Failed to find GTalk extension: ' + extension_path)

    extension = self.GetGTalkExtensionInfo()
    if extension:
      logging.info('Extension already installed. Skipping install...\n')
      return

    self.InstallExtension(extension_path, False)
    extension = self.GetGTalkExtensionInfo()
    self.assertTrue(extension, msg='Failed to install GTalk extension.')
    self.assertTrue(extension['is_enabled'], msg='GTalk extension is disabled.')

  def UninstallGTalkExtension(self):
    """Uninstall the GTalk extension (if present)"""
    extension = self.GetGTalkExtensionInfo()
    if extension:
      self.UninstallExtensionById(extension['id'])

  def GetGTalkExtensionInfo(self):
    """Get the data object about the GTalk extension."""
    extensions = [x for x in self.GetExtensionsInfo()
        if x['name'] == 'Chat for Google']
    return extensions[0] if len(extensions) == 1 else None

  def RunInMole(self, js, mole_index=0):
    """Execute javascript in a chat mole.

    Args:
      js: The javascript to run.
      mole_index: The index of the mole in which to run the JS.

    Returns:
      The resulting value from executing the javascript.
    """
    return self._RunInRenderView(self.GetMoleInfo(mole_index), js,
        '//iframe[1]')

  def RunInRoster(self, js):
    """Execute javascript in the chat roster.

    Args:
      js: The javascript to run.

    Returns:
      The resulting value from executing the javascript.
    """
    return self._RunInRenderView(self.GetViewerInfo(), js,
        '//iframe[1]\n//iframe[1]')

  def RunInLoginPage(self, js, xpath=''):
    """Execute javascript in the gaia login popup.

    Args:
      js: The javascript to run.
      xpath: The xpath to the frame in which to execute the javascript.

    Returns:
      The resulting value from executing the javascript.
    """
    return self._RunInTab(self.GetLoginPageInfo(), js, xpath)

  def RunInViewer(self, js, xpath=''):
    """Execute javascript in the GTalk viewer window.

    Args:
      js: The javascript to run.
      xpath: The xpath to the frame in which to execute the javascript.

    Returns:
      The resulting value from executing the javascript.
    """
    return self._RunInRenderView(self.GetViewerInfo(), js, xpath)

  def RunInBackground(self, js, xpath=''):
    """Execute javascript in the GTalk viewer window.

    Args:
      js: The javascript to run.
      xpath: The xpath to the frame in which to execute the javascript.

    Returns:
      The resulting value from executing the javascript.
    """
    background_view = self.GetBackgroundInfo()
    return self._RunInRenderView(background_view['view'], js, xpath)

  def GetMoleInfo(self, mole_index=0):
    """Get the data object about a given chat mole.

    Args:
      mole_index: The index of the mole to retrieve.

    Returns:
      Data object describing mole.
    """
    extension = self.GetGTalkExtensionInfo()
    return self._GetExtensionViewInfo(
        'chrome-extension://%s/panel.html' % extension['id'],
        mole_index)

  def GetViewerInfo(self):
    """Get the data object about the GTalk viewer dialog."""
    extension = self.GetGTalkExtensionInfo()
    return self._GetExtensionViewInfo(
        'chrome-extension://%s/viewer.html' % extension['id'])

  def GetLoginPageInfo(self):
    """Get the data object about the gaia login popup."""
    return self._GetTabInfo('https://accounts.google.com/ServiceLogin?')

  def GetBackgroundInfo(self):
    """Get the data object about the GTalk background page."""
    extension_views = self.GetBrowserInfo()['extension_views']
    for extension_view in extension_views:
      if 'Google Talk' in extension_view['name'] and \
          'EXTENSION_BACKGROUND_PAGE' == extension_view['view_type']:
        return extension_view
    return None

  def WaitUntilResult(self, result, func, msg):
    """Loop func until a condition matches is satified.

    Args:
      result: Value of func() at which to stop.
      func: Function to run at each iteration.
      msg: Error to print upon timing out.
    """
    assert callable(func)
    self.assertTrue(self.WaitUntil(
        lambda: func(), expect_retval=result), msg=msg)

  def WaitUntilCondition(self, func, matches, msg):
    """Loop func until condition matches is satified.

    Args:
      func: Function to run at each iteration.
      matches: Funtion to evalute output and determine whether to stop.
      msg: Error to print upon timing out.
    """
    assert callable(func)
    assert callable(matches)
    self.assertTrue(self.WaitUntil(
        lambda: matches(func())), msg=msg)

  def _WrapJs(self, statement):
    """Wrap the javascript to be executed.

    Args:
      statement: The piece of javascript to wrap.

    Returns:
      The wrapped javascript.
    """
    return """
            window.domAutomationController.send(
            (function(){
            %s
            try{return %s}
            catch(e){return "JS_ERROR: " + e}})())
           """ % (self._GetInjectedJs(), statement)

  def _RunInTab(self, tab, js, xpath=''):
    """Execute javascript in a given tab.

    Args:
      tab: The data object for the Chrome window tab returned by
          _GetTabInfo.
      js: The javascript to run.
      xpath: The xpath to the frame in which to execute the javascript.

    Returns:
      The resulting value from executing the javascript.
    """
    if not tab:
      logging.debug('Tab not found: %s' % tab)
      return False
    logging.info('Run in tab: %s' % js)

    value = self.ExecuteJavascript(
        self._WrapJs(js),
        tab_index = tab['index'],
        windex = tab['windex'],
        frame_xpath = xpath)
    self._LogRun(js, value)
    return value

  def _RunInRenderView(self, view, js, xpath=''):
    """Execute javascript in a given render view.

    Args:
      view: The data object for the Chrome render view returned by
          _GetExtensionViewInfo.
      js: The javascript to run.
      xpath: The xpath to the frame in which to execute the javascript.

    Returns:
      The resulting value from executing the javascript.
    """
    if not view:
      logging.debug('View not found: %s' % view)
      return False
    logging.info('Run in view: %s' % js)

    value = self.ExecuteJavascriptInRenderView(
        self._WrapJs(js),
        view,
        frame_xpath = xpath)
    self._LogRun(js, value)
    return value

  def _LogRun(self, js, value):
    """Log a particular run.

    Args:
      js: The javascript statement executed.
      value: The return value for the execution.
    """
    # works around UnicodeEncodeError: 'ascii' codec can't encode...
    out = value
    if not isinstance(value, basestring):
      out = str(value)
    out = re.sub('\s', ';', out[:300])
    logging.info(js + ' ===> ' + out.encode('utf-8'))

  def _GetTabInfo(self, url_query, index=0):
    """Get the data object for a given tab.

    Args:
      url_query: The substring of the URL to search for.
      index: The index within the list of matches to return.

    Returns:
      The data object for the tab.
    """
    windows = self.GetBrowserInfo()['windows']
    i = 0
    for win in windows:
      for tab in win['tabs']:
        if tab['url'] and url_query in tab['url']:
          # Store reference to windex used in _RunInTab.
          tab['windex'] = win['index']
          if i == index:
            return tab
          i = i + 1
    return None

  def _GetExtensionViewInfo(self, url_query, index=0):
    """Get the data object for a given extension view.

    Args:
      url_query: The substring of the URL to search for.
      index: The index within the list of matches to return.

    Returns:
      The data object for the tab.
    """
    extension_views = self.GetBrowserInfo()['extension_views']
    candidate_views = list()
    for extension_view in extension_views:
      if extension_view['url'] and url_query in extension_view['url']:
        candidate_views.append(extension_view['view'])

    # No guarantee on view order, so sort the views to get the correct one for
    # a given index.
    candidate_views.sort()
    if len(candidate_views) > index:
      return candidate_views[index]
    return None

  def _GetInjectedJs(self):
    """Get the javascript to inject in the execution environment."""
    if self._injected_js is None:
      self._injected_js = open(
          os.path.join(os.path.dirname(__file__), 'jsutils.js')).read()
    return self._injected_js
