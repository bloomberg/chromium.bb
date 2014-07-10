# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class MemoryTestsPage(page_module.Page):

  def __init__(self, page_set):
    super(MemoryTestsPage, self).__init__(
      url='file://../../data/gpu/mem_css3d.html', page_set=page_set,
      name='Memory.CSS3D')
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=60)


class MemoryTestsPageSet(page_set_module.PageSet):

  """ Tests that validate GPU memory management """

  def __init__(self):
    super(MemoryTestsPageSet, self).__init__(
      user_agent_type='desktop')

    self.AddPage(MemoryTestsPage(self))
