# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import test_expectations

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, mac, leopard, snowleopard, lion, mountainlion,
#     mavericks, yosemite, linux, chromeos, android
#
# GPU vendors:
#     amd, arm, broadcom, hisilicon, intel, imagination, nvidia, qualcomm,
#     vivante
#
# Browser types:
#     android-webview-shell, android-content-shell, debug
#
# ANGLE renderer:
#     d3d9, d3d11, opengl
#
# Specific GPUs can be listed as a tuple with vendor name and device ID.
# Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
# Device IDs must be paired with a GPU vendor.
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

ANGLE_MODIFIERS = ['d3d9', 'd3d11', 'opengl']

BROWSER_TYPE_MODIFIERS = [
    'android-webview-shell', 'android-content-shell', 'debug' ]

class FlakyExpectation(test_expectations.Expectation):
  def __init__(self, expectation, pattern, conditions=None, bug=None,
               max_num_retries=0):
    self.max_num_retries = max_num_retries
    self.angle_conditions = []
    self.browser_conditions = []
    super(FlakyExpectation, self).__init__(
      expectation, pattern, conditions=conditions, bug=bug)

  def ParseCondition(self, condition):
    """Adds support for ANGLE and browser type conditions.

    Browser types:
      android-webview-shell, android-content-shell, debug

    ANGLE renderer:
      d3d9, d3d11, opengl
    """
    # Add support for d3d9, d3d11 and opengl-specific expectations.
    if condition in ANGLE_MODIFIERS:
      self.angle_conditions.append(condition)
      return
    # Add support for browser-type-specific expectations.
    if condition in BROWSER_TYPE_MODIFIERS:
      self.browser_conditions.append(condition)
      return
    # Delegate to superclass.
    super(FlakyExpectation, self).ParseCondition(condition)


class GpuTestExpectations(test_expectations.TestExpectations):
  def CreateExpectation(self, expectation, url_pattern, conditions=None,
                        bug=None):
    return FlakyExpectation(expectation, url_pattern, conditions, bug)

  def Flaky(self, url_pattern, conditions=None, bug=None, max_num_retries=2):
    self.expectations.append(FlakyExpectation(
      'pass', url_pattern, conditions=conditions, bug=bug,
      max_num_retries=max_num_retries))

  def GetFlakyRetriesForPage(self, page, browser):
    e = self._GetExpectationObjectForPage(browser, page)
    if e:
      return e.max_num_retries
    return 0

  def ExpectationAppliesToPage(self, expectation, browser, page):
    if not super(GpuTestExpectations, self).ExpectationAppliesToPage(
        expectation, browser, page):
      return False

    # We'll only get here if the OS and GPU matched the expectation.

    # Check for presence of Android WebView.
    browser_matches = (
      (not expectation.browser_conditions) or
      browser.browser_type in expectation.browser_conditions)
    if not browser_matches:
      return False
    angle_renderer = ''
    gpu_info = None
    if browser.supports_system_info:
      gpu_info = browser.GetSystemInfo().gpu
    if gpu_info and gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer:
        if 'Direct3D11' in gl_renderer:
          angle_renderer = 'd3d11'
        elif 'Direct3D9' in gl_renderer:
          angle_renderer = 'd3d9'
        elif 'OpenGL' in gl_renderer:
          angle_renderer = 'opengl'
    angle_matches = (
      (not expectation.angle_conditions) or
      angle_renderer in expectation.angle_conditions)
    return angle_matches
