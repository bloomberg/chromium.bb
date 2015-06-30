# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.story import story_set as story_set_module


class MemoryTestsPage(page_module.Page):

  def __init__(self, story_set):
    super(MemoryTestsPage, self).__init__(
      url='file://../../data/gpu/mem_css3d.html', page_set=story_set,
      name='Memory.CSS3D')

  def RunNavigateSteps(self, action_runner):
    super(MemoryTestsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=60)


class MemoryTestsStorySet(story_set_module.StorySet):

  """ Tests that validate GPU memory management """

  def __init__(self):
    super(MemoryTestsStorySet, self).__init__()

    self.AddStory(MemoryTestsPage(self))
