# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cloud_storage_test_base
import optparse
import page_sets


test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
    if (msg.toLowerCase() == "success")
      domAutomationController._succeeded = true;
    else
      domAutomationController._succeeded = false;
  }

  window.domAutomationController = domAutomationController;
"""

def _DidTestSucceed(tab):
  return tab.EvaluateJavaScript('domAutomationController._succeeded')

class _GpuRasterizationValidator(cloud_storage_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-threaded-compositing',
                                    '--enable-impl-side-painting',
                                    '--force-gpu-rasterization',
                                    '--enable-gpu-benchmarking'])

  def ValidatePage(self, page, tab, results):
    if not _DidTestSucceed(tab):
      raise page_test.Failure('Page indicated a failure')

    if not hasattr(page, 'expectations') or not page.expectations:
      raise page_test.Failure('Expectations not specified')

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    screenshot = tab.Screenshot()
    if not screenshot:
      raise page_test.Failure('Could not capture screenshot')

    device_pixel_ratio = tab.EvaluateJavaScript('window.devicePixelRatio')
    if hasattr(page, 'test_rect'):
      test_rect = [int(x * device_pixel_ratio) for x in page.test_rect]
      screenshot = screenshot.Crop(
          test_rect[0], test_rect[1],
          test_rect[2], test_rect[3])

    self._ValidateScreenshotSamples(
        page.display_name,
        screenshot,
        page.expectations,
        device_pixel_ratio)


class GpuRasterization(cloud_storage_test_base.TestBase):
  """Tests that GPU rasterization produces valid content"""
  test = _GpuRasterizationValidator
  page_set = page_sets.GpuRasterizationTestsPageSet

  def CreatePageSet(self, options):
    page_set = super(GpuRasterization, self).CreatePageSet(options)
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
