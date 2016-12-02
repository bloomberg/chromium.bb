# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps pixel test.
Performs several common navigation actions on the map (pan, zoom, rotate) then
captures a screenshot and compares selected pixels against expected values"""

import json
import os

from gpu_tests import cloud_storage_test_base
from gpu_tests import gpu_test_base
from gpu_tests import maps_expectations
from gpu_tests import path_util

from telemetry.core import util
from telemetry.page import legacy_page_test
from telemetry import story as story_module
from telemetry.story import story_set as story_set_module


class MapsValidator(cloud_storage_test_base.ValidatorBase):
  def __init__(self):
    super(MapsValidator, self).__init__()

  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs(['--enable-gpu-benchmarking',
                                    '--test-type=gpu'])

  def ValidateAndMeasurePage(self, page, tab, results):
    # TODO: This should not be necessary, but it's not clear if the test is
    # failing on the bots in it's absence. Remove once we can verify that it's
    # safe to do so.
    MapsValidator.SpinWaitOnRAF(tab, 3)

    if not tab.screenshot_supported:
      raise legacy_page_test.Failure(
          'Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      raise legacy_page_test.Failure('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    print 'Maps\' devicePixelRatio is ' + str(dpr)
    # Even though the Maps test uses a fixed devicePixelRatio so that
    # it fetches all of the map tiles at the same resolution, on two
    # different devices with the same devicePixelRatio (a Retina
    # MacBook Pro and a Nexus 9), different scale factors of the final
    # screenshot are observed. Hack around this by specifying a scale
    # factor for these bots in the test expectations. This relies on
    # the test-machine-name argument being specified on the command
    # line.
    expected = self._ReadPixelExpectations(page)
    self._ValidateScreenshotSamples(
        tab, page.display_name, screenshot, expected, dpr)

  @staticmethod
  def SpinWaitOnRAF(tab, iterations, timeout=60):
    waitScript = r"""
      window.__spinWaitOnRAFDone = false;
      var iterationsLeft = %d;

      function spin() {
        iterationsLeft--;
        if (iterationsLeft == 0) {
          window.__spinWaitOnRAFDone = true;
          return;
        }
        window.requestAnimationFrame(spin);
      }
      window.requestAnimationFrame(spin);
    """ % iterations

    def IsWaitComplete():
      return tab.EvaluateJavaScript('window.__spinWaitOnRAFDone')

    tab.ExecuteJavaScript(waitScript)
    util.WaitFor(IsWaitComplete, timeout)

  def _ReadPixelExpectations(self, page):
    expectations_path = os.path.join(page._base_dir, page.pixel_expectations)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents


class MapsPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(MapsPage, self).__init__(
        url='http://map-test/performance.html',
        page_set=story_set,
        base_dir=base_dir,
        name='Maps.maps_004',
        make_javascript_deterministic=False,
        expectations=expectations)
    self.pixel_expectations = 'data/maps_004_expectations.json'

  def RunNavigateSteps(self, action_runner):
    super(MapsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.testDone', timeout_in_seconds=180)


class Maps(cloud_storage_test_base.CloudStorageTestBase):
  """Google Maps pixel tests.

  Note: the WPR for this test was recorded from the smoothness.maps
  benchmark's similar page. The Maps team gave us a build of their
  test. The only modification to the test was to config.js, where the
  width and height query args were set to 800 by 600. The WPR was
  recorded with:

  tools/perf/record_wpr smoothness_maps --browser=system

  This would produce maps_???.wpr and maps.json were copied from
  tools/perf/page_sets/data into content/test/gpu/page_sets/data.
  It worths noting that telemetry no longer allows replaying URL that has form
  of local host. If the recording was created for locahost URL, ones can update
  the host name by running:
    web-page-replay/httparchive.py remap-host maps_004.wpr \
    localhost:10020 map-test
  (web-page-replay/ can be found in third_party/catapult/telemetry/third_party/)
  After update the host name in WPR archive, please remember to update the host
  URL in content/test/gpu/gpu_tests/maps.py as well.

  To upload the maps_???.wpr to cloud storage, one would run:
    depot_tools/upload_to_google_storage.py --bucket=chromium-telemetry \
    maps_???.wpr
  The same sha1 file and json file need to be copied into both of these
  directories in any CL which updates the recording."""
  test = MapsValidator

  @classmethod
  def Name(cls):
    return 'maps'

  def _CreateExpectations(self):
    return maps_expectations.MapsExpectations()

  def CreateStorySet(self, options):
    story_set_path = os.path.join(
        path_util.GetChromiumSrcDir(), 'content', 'test', 'gpu', 'page_sets')
    ps = story_set_module.StorySet(
        archive_data_file='data/maps.json',
        base_dir=story_set_path,
        cloud_storage_bucket=story_module.PUBLIC_BUCKET)
    ps.AddStory(MapsPage(ps, ps.base_dir, self.GetExpectations()))
    return ps
