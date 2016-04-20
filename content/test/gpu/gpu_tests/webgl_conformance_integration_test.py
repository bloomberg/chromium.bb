# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests import webgl_conformance

from telemetry.testing import serially_executed_browser_test_case


class WebGLConformanceIntegrationTest(
    serially_executed_browser_test_case.SeriallyBrowserTestCase):

  _webgl_version = None

  @classmethod
  def Name(cls):
    return 'webgl_conformance'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    parser.add_option('--webgl-conformance-version',
        help='Version of the WebGL conformance tests to run.',
        default='1.0.4')
    parser.add_option('--webgl2-only',
        help='Whether we include webgl 1 tests if version is 2.0.0 or above.',
        default='false')

  @classmethod
  def GenerateTestCases_RunWebGLTests(cls, options):
    test_paths = webgl_conformance.WebglConformance._ParseTests(
        '00_test_list.txt',
        options.webgl_conformance_version,
        (options.webgl2_only == 'true'),
        None)
    cls._webgl_version = [
        int(x) for x in options.webgl_conformance_version.split('.')][0]
    for test in test_paths:
      # generated test name cannot contain '.'
      name = webgl_conformance.GenerateTestNameFromTestPath(test).replace(
          '.', '_')
      yield name, (test, )

  def RunWebGLTests(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    harness_script = webgl_conformance.conformance_harness_script
    self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)

    if not webgl_conformance._DidWebGLTestSucceed(self.tab):
      self.fail(webgl_conformance._WebGLTestMessages(self.tab))

  @classmethod
  def CustomizeOptions(cls):
    validator = webgl_conformance.WebglConformanceValidator()
    validator.CustomizeBrowserOptions(cls._finder_options.browser_options)

  @classmethod
  def setUpClass(cls):
    super(cls, WebGLConformanceIntegrationTest).setUpClass()
    cls.CustomizeOptions()
    cls.StartBrowser(cls._finder_options)
    cls.tab = cls._browser.tabs[0]
    cls.SetStaticServerDir(webgl_conformance.conformance_path)
