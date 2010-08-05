#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import pyauto_functional
from pyauto import PyUITest


class ExecuteJavascriptTest(PyUITest):

  def testExecuteJavascript(self):
    path = os.path.join(self.DataDir(), "frame_dom_access",
                       "frame_dom_access.html")

    self.NavigateToURL(self.GetFileURLForPath(path))

    v = self.ExecuteJavascript("window.domAutomationController.send(" +
                               "document.getElementById('myinput').nodeName)")
    self.assertEqual(v, "INPUT")

  def testGetDOMValue(self):
    path = os.path.join(self.DataDir(), "frame_dom_access",
                       "frame_dom_access.html")

    self.NavigateToURL(self.GetFileURLForPath(path))

    v = self.GetDOMValue("document.getElementById('myinput').nodeName")
    self.assertEqual(v, "INPUT")


if __name__ == '__main__':
  pyauto_functional.Main()
