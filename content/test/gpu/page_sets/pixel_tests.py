# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.story import story_set as story_set_module


class PixelTestsPage(page_module.Page):

  def __init__(self, url, name, test_rect, revision, story_set):
    super(PixelTestsPage, self).__init__(url=url, page_set=story_set, name=name)
    self.test_rect = test_rect
    self.revision = revision

  def RunNavigateSteps(self, action_runner):
    super(PixelTestsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class PixelTestsStorySet(story_set_module.StorySet):

  """ Some basic test cases for GPU. """

  def __init__(self, base_name='Pixel'):
    super(PixelTestsStorySet, self).__init__()
    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_canvas2d.html',
      name=base_name + '.Canvas2DRedBox',
      test_rect=[0, 0, 300, 300],
      revision=5,
      story_set=self))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_css3d.html',
      name=base_name + '.CSS3DBlueBox',
      test_rect=[0, 0, 300, 300],
      revision=13,
      story_set=self))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_webgl.html',
      name=base_name + '.WebGLGreenTriangle',
      test_rect=[0, 0, 300, 300],
      revision=10,
      story_set=self))
