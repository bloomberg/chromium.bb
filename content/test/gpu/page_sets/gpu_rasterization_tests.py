# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GpuRasterizationTestsPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, page_set):
    super(GpuRasterizationTestsPage, self).__init__(
      url='file://../../data/gpu/pixel_css3d.html',
      page_set=page_set)

    self.user_agent_type = 'desktop'
    self.name = 'GpuRasterization.CSS3DBlueBox'
    self.expectations = [
      {'comment': 'body-t',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 5]},
      {'comment': 'body-r',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [245, 5]},
      {'comment': 'body-b',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [245, 245]},
      {'comment': 'body-l',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 245]},
      {'comment': 'background-t',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [30, 30]},
      {'comment': 'background-r',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [170, 30]},
      {'comment': 'background-b',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [170, 170]},
      {'comment': 'background-l',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [30, 170]},
      {'comment': 'box-t',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [70, 70]},
      {'comment': 'box-r',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [140, 70]},
      {'comment': 'box-b',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [140, 120]},
      {'comment': 'box-l',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [70, 120]}
    ]
    self.test_rect = [0, 0, 250, 250]

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'domAutomationController._finished',
        'timeout': 30
      }))


class GpuRasterizationTestsPageSet(page_set_module.PageSet):

  """ Basic test cases for GPU rasterization. """

  def __init__(self):
    super(GpuRasterizationTestsPageSet, self).__init__(
      user_agent_type='desktop')

    self.AddPage(GpuRasterizationTestsPage(self))
