# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.story import story_set as story_set_module

from gpu_tests import gpu_test_base

class GpuRasterizationBlueBoxPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(GpuRasterizationBlueBoxPage, self).__init__(
      url='file://../../data/gpu/pixel_background.html',
      page_set=story_set,
      name='GpuRasterization.BlueBox',
      expectations=expectations)

    self.expectations = [
      {'comment': 'body-t',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 5],
       'size': [1, 1]},
      {'comment': 'body-r',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [215, 5],
       'size': [1, 1]},
      {'comment': 'body-b',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [215, 215],
       'size': [1, 1]},
      {'comment': 'body-l',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 215],
       'size': [1, 1]},
      {'comment': 'background-t',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [30, 30],
       'size': [1, 1]},
      {'comment': 'background-r',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [170, 30],
       'size': [1, 1]},
      {'comment': 'background-b',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [170, 170],
       'size': [1, 1]},
      {'comment': 'background-l',
       'color': [0, 0, 0],
       'tolerance': 0,
       'location': [30, 170],
       'size': [1, 1]},
      {'comment': 'box-t',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [70, 70],
       'size': [1, 1]},
      {'comment': 'box-r',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [140, 70],
       'size': [1, 1]},
      {'comment': 'box-b',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [140, 140],
       'size': [1, 1]},
      {'comment': 'box-l',
       'color': [0, 0, 255],
       'tolerance': 0,
       'location': [70, 140],
       'size': [1, 1]}
    ]
    self.test_rect = [0, 0, 220, 220]

  def RunNavigateSteps(self, action_runner):
    super(GpuRasterizationBlueBoxPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class GpuRasterizationConcavePathsPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(GpuRasterizationConcavePathsPage, self).__init__(
      url='file://../../data/gpu/concave_paths.html',
      page_set=story_set,
      name='GpuRasterization.ConcavePaths',
      expectations=expectations)

    self.expectations = [
      {'comment': 'outside',
       'color': [255, 255, 255],
       'tolerance': 0,
       'location': [5, 5],
       'size': [1, 1]},
      {'comment': 'inside',
       'color': [255, 215, 0],
       'tolerance': 0,
       'location': [20, 50],
       'size': [1, 1]}
    ]
    self.test_rect = [0, 0, 100, 100]

  def RunNavigateSteps(self, action_runner):
    super(GpuRasterizationConcavePathsPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)

class GpuRasterizationTestsStorySet(story_set_module.StorySet):

  """ Basic test cases for GPU rasterization. """

  def __init__(self, expectations):
    super(GpuRasterizationTestsStorySet, self).__init__()

    self.AddStory(GpuRasterizationBlueBoxPage(self, expectations))
    self.AddStory(GpuRasterizationConcavePathsPage(self, expectations))
