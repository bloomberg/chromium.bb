# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests import gpu_integration_test
from gpu_tests import webgl_conformance
from gpu_tests import webgl_conformance_expectations
from gpu_tests import webgl2_conformance_expectations


class WebGLConformanceIntegrationTest(gpu_integration_test.GpuIntegrationTest):

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
  def GenerateGpuTests(cls, options):
    test_paths = webgl_conformance.WebglConformance._ParseTests(
        '00_test_list.txt',
        options.webgl_conformance_version,
        (options.webgl2_only == 'true'),
        None)
    cls._webgl_version = [
        int(x) for x in options.webgl_conformance_version.split('.')][0]
    for test_path in test_paths:
      # generated test name cannot contain '.'
      name = webgl_conformance.GenerateTestNameFromTestPath(test_path).replace(
          '.', '_')
      yield (name, test_path, ())

  def RunActualGpuTest(self, test_path, *args):
    url = self.UrlOfStaticFilePath(test_path)
    harness_script = webgl_conformance.conformance_harness_script
    self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
    self.tab.action_runner.WaitForJavaScriptCondition(
        'webglTestHarness._finished', timeout_in_seconds=300)
    if not webgl_conformance._DidWebGLTestSucceed(self.tab):
      self.fail(webgl_conformance._WebGLTestMessages(self.tab))

  @classmethod
  def CustomizeOptions(cls):
    assert cls._webgl_version == 1 or cls._webgl_version == 2
    validator = None
    if cls._webgl_version == 1:
      validator = webgl_conformance.WebglConformanceValidator()
    else:
      validator = webgl_conformance.Webgl2ConformanceValidator()
    validator.CustomizeBrowserOptions(cls._finder_options.browser_options)

  @classmethod
  def _CreateExpectations(cls):
    assert cls._webgl_version == 1 or cls._webgl_version == 2
    if cls._webgl_version == 1:
      return webgl_conformance_expectations.WebGLConformanceExpectations(
          webgl_conformance.conformance_path)
    else:
      return webgl2_conformance_expectations.WebGL2ConformanceExpectations(
          webgl_conformance.conformance_path)

  @classmethod
  def setUpClass(cls):
    super(cls, WebGLConformanceIntegrationTest).setUpClass()
    cls.CustomizeOptions()
    cls.StartBrowser(cls._finder_options)
    cls.SetStaticServerDir(webgl_conformance.conformance_path)
