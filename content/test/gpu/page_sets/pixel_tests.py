# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PixelTestsPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, name, test_rect, revision, page_set):
    super(PixelTestsPage, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.name = name
    self.test_rect = test_rect
    self.revision = revision

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'domAutomationController._finished',
        'timeout': 30
      }))


class PixelTestsPageSet(page_set_module.PageSet):

  """ Some basic test cases for GPU. """

  def __init__(self):
    super(PixelTestsPageSet, self).__init__(
      user_agent_type='desktop')
    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_canvas2d.html',
      name='Pixel.Canvas2DRedBox',
      test_rect=[0, 0, 400, 300],
      revision=2,
      page_set=self))

    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_css3d.html',
      name='Pixel.CSS3DBlueBox',
      test_rect=[0, 0, 400, 300],
      revision=3,
      page_set=self))

    self.AddPage(PixelTestsPage(
      url='file://../../data/gpu/pixel_webgl.html',
      name='Pixel.WebGLGreenTriangle',
      test_rect=[0, 0, 400, 300],
      revision=3,
      page_set=self))
