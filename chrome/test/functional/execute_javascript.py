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


if __name__ == '__main__':
  pyauto_functional.Main()
