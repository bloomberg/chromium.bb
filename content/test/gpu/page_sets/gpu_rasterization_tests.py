# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GpuRasterizationBlueBoxPage(page_module.Page):

  def __init__(self, page_set):
    super(GpuRasterizationBlueBoxPage, self).__init__(
      url='file://../../data/gpu/pixel_background.html',
      page_set=page_set,
      name='GpuRasterization.BlueBox')

    self.expectations = [
      {'comment': 'body-t',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 5]},
      {'comment': 'body-r',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [215, 5]},
      {'comment': 'body-b',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [215, 215]},
      {'comment': 'body-l',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 215]},
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
       'location': [140, 140]},
      {'comment': 'box-l',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [70, 140]}
    ]
    self.test_rect = [0, 0, 220, 220]

  def RunNavigateSteps(self, action_runner):
    super(GpuRasterizationBlueBoxPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class GpuRasterizationConcavePathsPage(page_module.Page):

  def __init__(self, page_set):
    super(GpuRasterizationConcavePathsPage, self).__init__(
      url='file://../../data/gpu/concave_paths.html',
      page_set=page_set,
      name='GpuRasterization.ConcavePaths')

    self.expectations = [
      {'comment': 'outside',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 5]},
      {'comment': 'inside',
       'color': [255, 215, 0],
       'tolerance': 0,
       'location': [20, 50]},
      {'comment': 'edge-1',
       'color': [255, 220, 32],
       'tolerance': 32,
       'location': [72, 50]},
    ]
    self.test_rect = [0, 0, 100, 100]

  def RunNavigateSteps(self, action_runner):
    super(GpuRasterizationConcavePathsPage, self).\
      RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)

class GpuRasterizationTestsPageSet(page_set_module.PageSet):

  """ Basic test cases for GPU rasterization. """

  def __init__(self):
    super(GpuRasterizationTestsPageSet, self).__init__()

    self.AddUserStory(GpuRasterizationBlueBoxPage(self))
    self.AddUserStory(GpuRasterizationConcavePathsPage(self))
