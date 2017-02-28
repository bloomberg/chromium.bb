# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import maps_expectations
from gpu_tests import path_util

from py_utils import cloud_storage

data_path = os.path.join(path_util.GetChromiumSrcDir(),
                         'content', 'test', 'gpu', 'page_sets', 'data')

class MapsIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):
  """Google Maps pixel tests.

  Note: the WPR for this test was recorded from the smoothness.maps
  benchmark's similar page. The Maps team gave us a build of their test.  The
  only modification to the test was to config.js, where the width and height
  query args were set to 800 by 600. The WPR was recorded with:

  tools/perf/record_wpr smoothness_maps --browser=system

  This produced maps_???.wpr and maps.json, which were copied from
  tools/perf/page_sets/data into content/test/gpu/page_sets/data.

  It's worth noting that telemetry no longer allows replaying a URL that
  refers to localhost. If the recording was created for the locahost URL, one
  can update the host name by running:

    web-page-replay/httparchive.py remap-host maps_004.wpr \
    localhost:10020 map-test

  (web-page-replay/ can be found in third_party/catapult/telemetry/third_party/)

  After updating the host name in the WPR archive, please remember to update
  the host URL in content/test/gpu/gpu_tests/maps_integration_test.py as well.

  To upload the maps_???.wpr to cloud storage, one would run:

    depot_tools/upload_to_google_storage.py --bucket=chromium-telemetry \
    maps_???.wpr

  The same sha1 file and json file need to be copied into both of these
  directories in any CL which updates the recording.
  """

  @classmethod
  def Name(cls):
    return 'maps'

  @classmethod
  def _CreateExpectations(cls):
    return maps_expectations.MapsExpectations()

  @classmethod
  def SetUpProcess(cls):
    super(cls, MapsIntegrationTest).SetUpProcess()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartWPRServer(os.path.join(data_path, 'maps_004.wpr.updated'),
                       cloud_storage.PUBLIC_BUCKET)
    cls.StartBrowser()

  @classmethod
  def TearDownProcess(cls):
    super(cls, MapsIntegrationTest).TearDownProcess()
    cls.StopWPRServer()

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    yield('Maps_maps_004',
          'http://map-test/performance.html',
          ('maps_004_expectations.json'))

  def _ReadPixelExpectations(self, expectations_file):
    expectations_path = os.path.join(data_path, expectations_file)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents

  def _SpinWaitOnRAF(self, iterations, timeout=60):
    self.tab.ExecuteJavaScript("""
        window.__spinWaitOnRAFDone = false;
        var iterationsLeft = {{ iterations }};

        function spin() {
          iterationsLeft--;
          if (iterationsLeft == 0) {
            window.__spinWaitOnRAFDone = true;
            return;
          }
          window.requestAnimationFrame(spin);
        }
        window.requestAnimationFrame(spin);
        """, iterations=iterations)
    self.tab.WaitForJavaScriptCondition(
        'window.__spinWaitOnRAFDone', timeout=timeout)

  def RunActualGpuTest(self, url, *args):
    tab = self.tab
    pixel_expectations_file = args[0]
    action_runner = tab.action_runner
    action_runner.Navigate(url)
    action_runner.WaitForJavaScriptCondition(
        'window.testDone', timeout=180)

    # TODO(kbr): This should not be necessary, but it's not clear if the test
    # is failing on the bots in its absence. Remove once we can verify that
    # it's safe to do so.
    self._SpinWaitOnRAF(3)

    if not tab.screenshot_supported:
      self.fail('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')

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
    expected = self._ReadPixelExpectations(pixel_expectations_file)
    self._ValidateScreenshotSamples(tab, url, screenshot, expected, dpr)

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
