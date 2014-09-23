# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PixelTestsPage(page_module.Page):

  def __init__(self, url, name, test_rect, revision, page_set):
    super(PixelTestsPage, self).__init__(url=url, page_set=page_set, name=name)
    self.user_agent_type = 'desktop'
    self.test_rect = test_rect
    self.revision = revision

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class PixelTestsPageSet(page_set_module.PageSet):

  """ Some basic test cases for GPU. """

  def __init__(self):
    super(PixelTestsPageSet, self).__init__(
      user_agent_type='desktop')
    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_canvas2d.html',
      name='Pixel.Canvas2DRedBox',
      test_rect=[0, 0, 300, 300],
      revision=4,
      page_set=self))

    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_css3d.html',
      name='Pixel.CSS3DBlueBox',
      test_rect=[0, 0, 300, 300],
      revision=11,
      page_set=self))

    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_webgl.html',
      name='Pixel.WebGLGreenTriangle',
      test_rect=[0, 0, 300, 300],
      revision=8,
      page_set=self))
