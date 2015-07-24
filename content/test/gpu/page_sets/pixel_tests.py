# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.story import story_set as story_set_module

from gpu_tests import gpu_test_base

class PixelTestsPage(gpu_test_base.PageBase):

  def __init__(self, url, name, test_rect, revision, story_set, expectations):
    super(PixelTestsPage, self).__init__(url=url, page_set=story_set, name=name,
                                         expectations=expectations)
    self.test_rect = test_rect
    self.revision = revision

  def RunNavigateStepsInner(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class PixelTestsStorySet(story_set_module.StorySet):

  """ Some basic test cases for GPU. """

  def __init__(self, expectations, base_name='Pixel'):
    super(PixelTestsStorySet, self).__init__()
    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_canvas2d.html',
      name=base_name + '.Canvas2DRedBox',
      test_rect=[0, 0, 300, 300],
      revision=6,
      story_set=self,
      expectations=expectations))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_css3d.html',
      name=base_name + '.CSS3DBlueBox',
      test_rect=[0, 0, 300, 300],
      revision=14,
      story_set=self,
      expectations=expectations))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_webgl.html',
      name=base_name + '.WebGLGreenTriangle',
      test_rect=[0, 0, 300, 300],
      revision=11,
      story_set=self,
      expectations=expectations))
