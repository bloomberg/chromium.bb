# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.story import story_set as story_set_module

class GpuProcessTestsPage(page_module.Page):

  def __init__(self, url, name, story_set):
    super(GpuProcessTestsPage, self).__init__(url=url, page_set=story_set,
                                              name=name)


class FunctionalVideoPage(GpuProcessTestsPage):

  def __init__(self, story_set):
    super(FunctionalVideoPage, self).__init__(
      url='file://../../data/gpu/functional_video.html',
      name='GpuProcess.video',
      story_set=story_set)

  def RunNavigateSteps(self, action_runner):
    super(FunctionalVideoPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class GpuInfoCompletePage(GpuProcessTestsPage):

  def __init__(self, story_set):
    super(GpuInfoCompletePage, self).__init__(
      url='file://../../data/gpu/functional_3d_css.html',
      name='GpuProcess.gpu_info_complete',
      story_set=story_set)

  def Validate(self, tab, results):
    # Regression test for crbug.com/454906
    if not tab.browser.supports_system_info:
      raise page_test.Failure('Browser must support system info')
    system_info = tab.browser.GetSystemInfo()
    if not system_info.gpu:
      raise page_test.Failure('Target machine must have a GPU')
    if not system_info.gpu.aux_attributes:
      raise page_test.Failure('Browser must support GPU aux attributes')
    if not 'gl_renderer' in system_info.gpu.aux_attributes:
      raise page_test.Failure('Browser must have gl_renderer in aux attribs')
    if len(system_info.gpu.aux_attributes['gl_renderer']) <= 0:
      raise page_test.Failure('Must have a non-empty gl_renderer string')


class GpuProcessTestsStorySet(story_set_module.StorySet):

  """ Tests that accelerated content triggers the creation of a GPU process """

  def __init__(self):
    super(GpuProcessTestsStorySet, self).__init__(
      serving_dirs=set(['../../../../content/test/data']))

    urls_and_names_list = [
      ('file://../../data/gpu/functional_canvas_demo.html',
       'GpuProcess.canvas2d'),
      ('file://../../data/gpu/functional_3d_css.html',
       'GpuProcess.css3d'),
      ('file://../../data/gpu/functional_webgl.html',
       'GpuProcess.webgl')
    ]

    for url, name in urls_and_names_list:
      self.AddStory(GpuProcessTestsPage(url, name, self))

    self.AddStory(FunctionalVideoPage(self))
    self.AddStory(GpuInfoCompletePage(self))
