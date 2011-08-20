#!/usr/bin/python
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
    ext_id = self.InstallExtension(dir_path, False);
    self.assertTrue(ext_id, msg='Failed to install extension: %s.' % dir_path)

    # Verify extension is enabled.
    extension = self._GetExtensionInfoById(self.GetExtensionsInfo(), ext_id)
    self.assertTrue(extension['is_enabled'],
                    msg='Extension was disabled by default')

    # Get the background page's view.
    info = self.GetBrowserInfo()['extension_views']
    view = [x for x in info if
            x['extension_id'] == ext_id and
            x['view_type'] == 'EXTENSION_BACKGROUND_PAGE']
    self.assertEqual(1, len(view),
                     msg='problematic background view: view = %s.' % view)
    background_view = view[0]

    # Get a value from background page's DOM
    v = self.ExecuteJavascriptInRenderView(
        'window.domAutomationController.send('
        'document.getElementById("myinput").nodeName)', background_view['view'])
    self.assertEqual(v, 'INPUT', msg='Incorrect value returned.')


if __name__ == '__main__':
  pyauto_functional.Main()
