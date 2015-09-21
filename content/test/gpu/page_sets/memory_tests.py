# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.story import story_set as story_set_module

from gpu_tests import gpu_test_base

class MemoryTestsPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(MemoryTestsPage, self).__init__(
      url='file://../../data/gpu/mem_css3d.html', page_set=story_set,
      name='Memory.CSS3D',
      expectations=expectations)

  def RunNavigateSteps(self, action_runner):
    super(MemoryTestsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=60)


class MemoryTestsStorySet(story_set_module.StorySet):

  """ Tests that validate GPU memory management """

  def __init__(self, expectations):
    super(MemoryTestsStorySet, self).__init__()

    self.AddStory(MemoryTestsPage(self, expectations))
