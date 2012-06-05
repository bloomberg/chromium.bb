#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import test_urlfetch

from template_utility import TemplateUtility
from third_party.handlebar import Handlebar

def _ReadFile(filename):
  with open(os.path.join('test_data', 'template_utility', filename), 'r') as f:
    return f.read()

class TemplateUtilityTest(unittest.TestCase):
  def _RenderTest(self, base):
    template = Handlebar(_ReadFile(base + '_tmpl.html'))
    self.assertEquals(_ReadFile(base + '.html'),
        self.t_util.RenderTemplate(template, _ReadFile(base + '.json')))

  def testRenderTemplate(self):
    self.t_util = TemplateUtility()
    self._RenderTest('test1')
    self._RenderTest('test2')

if __name__ == '__main__':
  unittest.main()
