#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import pyauto_functional
from pyauto import PyUITest


class ExecuteJavascriptTest(PyUITest):
  def _GetExtensionInfoById(self, extensions, id):
    for x in extensions:
      if x['id'] == id:
        return x
    return None

  def testExecuteJavascript(self):
    self.NavigateToURL(self.GetFileURLForDataPath(
        'frame_dom_access', 'frame_dom_access.html'))

    v = self.ExecuteJavascript('window.domAutomationController.send(' +
                               'document.getElementById("myinput").nodeName)')
    self.assertEqual(v, 'INPUT')

  def testGetDOMValue(self):
    self.NavigateToURL(self.GetFileURLForDataPath(
        'frame_dom_access', 'frame_dom_access.html'))

    v = self.GetDOMValue('document.getElementById("myinput").nodeName')
    self.assertEqual(v, 'INPUT')

  def testExecuteJavascriptInExtension(self):
    """Test we can inject JavaScript into an extension."""
    dir_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'js_injection_background'))
    ext_id = self.InstallExtension(dir_path)

    # Verify extension is enabled.
    extension = self._GetExtensionInfoById(self.GetExtensionsInfo(), ext_id)
    self.assertTrue(extension['is_enabled'],
                    msg='Extension was disabled by default')

    # Get the background page's view.
    background_view = self.WaitUntilExtensionViewLoaded(
        view_type='EXTENSION_BACKGROUND_PAGE')
    self.assertTrue(background_view,
                    msg='problematic background view: views = %s.' %
                    self.GetBrowserInfo()['extension_views'])

    # Get values from background page's DOM
    v = self.ExecuteJavascriptInRenderView(
        'window.domAutomationController.send('
        'document.getElementById("myinput").nodeName)', background_view)
    self.assertEqual(v, 'INPUT',
                     msg='Incorrect value returned (v = %s).' % v)
    v = self.ExecuteJavascriptInRenderView(
        'window.domAutomationController.send(bool_var)', background_view)
    self.assertEqual(v, True, msg='Incorrect value returned (v = %s).' % v)
    v = self.ExecuteJavascriptInRenderView(
        'window.domAutomationController.send(int_var)', background_view)
    self.assertEqual(v, 42, msg='Incorrect value returned (v = %s).' % v)
    v = self.ExecuteJavascriptInRenderView(
        'window.domAutomationController.send(str_var)', background_view)
    self.assertEqual(v, 'foo', msg='Incorrect value returned (v = %s).' % v)


if __name__ == '__main__':
  pyauto_functional.Main()
