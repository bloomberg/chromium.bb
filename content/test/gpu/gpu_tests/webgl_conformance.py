# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from gpu_tests import gpu_test_base
from gpu_tests import path_util
from gpu_tests import webgl_conformance_expectations
from gpu_tests import webgl2_conformance_expectations

from telemetry.internal.browser import browser_finder
from telemetry.page import page_test
from telemetry.story.story_set import StorySet


conformance_path = os.path.join(
    path_util.GetChromiumSrcDir(),
    'third_party', 'webgl', 'src', 'sdk', 'tests')

conformance_harness_script = r"""
  var testHarness = {};
  testHarness._allTestSucceeded = true;
  testHarness._messages = '';
  testHarness._failures = 0;
  testHarness._finished = false;
  testHarness._originalLog = window.console.log;

  testHarness.log = function(msg) {
    testHarness._messages += msg + "\n";
    testHarness._originalLog.apply(window.console, [msg]);
  }

  testHarness.reportResults = function(url, success, msg) {
    testHarness._allTestSucceeded = testHarness._allTestSucceeded && !!success;
    if(!success) {
      testHarness._failures++;
      if(msg) {
        testHarness.log(msg);
      }
    }
  };
  testHarness.notifyFinished = function(url) {
    testHarness._finished = true;
  };
  testHarness.navigateToPage = function(src) {
    var testFrame = document.getElementById("test-frame");
    testFrame.src = src;
  };

  window.webglTestHarness = testHarness;
  window.parent.webglTestHarness = testHarness;
  window.console.log = testHarness.log;
  window.onerror = function(message, url, line) {
    testHarness.reportResults(null, false, message);
    testHarness.notifyFinished(null);
  };
  window.quietMode = function() { return true; }
"""

def _DidWebGLTestSucceed(tab):
  return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded')

def _WebGLTestMessages(tab):
  return tab.EvaluateJavaScript('webglTestHarness._messages')

def _CompareVersion(version1, version2):
  ver_num1 = [int(x) for x in version1.split('.')]
  ver_num2 = [int(x) for x in version2.split('.')]
  size = min(len(ver_num1), len(ver_num2))
  return cmp(ver_num1[0:size], ver_num2[0:size])

class WebglConformanceValidator(gpu_test_base.ValidatorBase):
  def __init__(self):
    super(WebglConformanceValidator, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    if not _DidWebGLTestSucceed(tab):
      raise page_test.Failure(_WebGLTestMessages(tab))

  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs([
        '--disable-gesture-requirement-for-media-playback',
        '--disable-domain-blocking-for-3d-apis',
        '--disable-gpu-process-crash-limit',
        '--js-flags=--expose-gc',
        '--test-type=gpu',
        '--enable-experimental-canvas-features'
    ])
    browser = browser_finder.FindBrowser(options.finder_options)
    if (browser.target_os.startswith('android') and
      browser.browser_type == 'android-webview-shell'):
      # TODO(kbr): this is overly broad. We'd like to do this only on
      # Nexus 9. It'll go away shortly anyway. crbug.com/499928
      #
      # The --ignore_egl_sync_failures is only there to work around
      # some strange failure on the Nexus 9 bot, not reproducible on
      # local hardware.
      options.AppendExtraBrowserArgs([
        '--disable-gl-extensions=GL_EXT_disjoint_timer_query',
        '--ignore_egl_sync_failures',
      ])


class Webgl2ConformanceValidator(WebglConformanceValidator):
  def __init__(self):
    super(Webgl2ConformanceValidator, self).__init__()

  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs([
        '--disable-gesture-requirement-for-media-playback',
        '--disable-domain-blocking-for-3d-apis',
        '--disable-gpu-process-crash-limit',
        '--js-flags=--expose-gc',
        '--enable-unsafe-es3-apis',
        '--test-type=gpu',
        '--enable-experimental-canvas-features'
    ])
    browser = browser_finder.FindBrowser(options.finder_options)
    if browser.target_os == 'darwin':
      # crbug.com/539993
      options.AppendExtraBrowserArgs([
          '--disable-accelerated-video-decode'
      ])

class WebglConformancePage(gpu_test_base.PageBase):
  def __init__(self, story_set, test, expectations):
    super(WebglConformancePage, self).__init__(
      url='file://' + test, page_set=story_set, base_dir=story_set.base_dir,
      shared_page_state_class=gpu_test_base.DesktopGpuSharedPageState,
      name=('WebglConformance.%s' %
              test.replace('/', '_').replace('-', '_').
                 replace('\\', '_').rpartition('.')[0].replace('.', '_')),
      expectations=expectations)
    self.script_to_evaluate_on_commit = conformance_harness_script

  def RunNavigateSteps(self, action_runner):
    super(WebglConformancePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'webglTestHarness._finished', timeout_in_seconds=180)

class WebglConformance(gpu_test_base.TestBase):
  """Conformance with Khronos WebGL Conformance Tests"""
  def __init__(self):
    super(WebglConformance, self).__init__(max_failures=10)
    self._cached_expectations = None
    self._webgl_version = 0

  @classmethod
  def Name(cls):
    return 'webgl_conformance'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, group):
    group.add_option('--webgl-conformance-version',
        help='Version of the WebGL conformance tests to run.',
        default='1.0.4')
    group.add_option('--webgl2-only',
        help='Whether we include webgl 1 tests if version is 2.0.0 or above.',
        default='false')

  def CreatePageTest(self, options):
    if _CompareVersion(options.webgl_conformance_version, '2.0.0') >= 0:
      return Webgl2ConformanceValidator()
    return WebglConformanceValidator()

  def CreateStorySet(self, options):
    tests = self._ParseTests('00_test_list.txt',
        options.webgl_conformance_version,
        (options.webgl2_only == 'true'),
        None)

    self._webgl_version = [
        int(x) for x in options.webgl_conformance_version.split('.')][0]

    ps = StorySet(serving_dirs=[''], base_dir=conformance_path)

    expectations = self.GetExpectations()
    for test in tests:
      ps.AddStory(WebglConformancePage(ps, test, expectations))

    return ps

  def _CreateExpectations(self):
    assert self._webgl_version == 1 or self._webgl_version == 2
    if self._webgl_version == 1:
      return webgl_conformance_expectations.WebGLConformanceExpectations(
          conformance_path)
    else:
      return webgl2_conformance_expectations.WebGL2ConformanceExpectations(
          conformance_path)

  @staticmethod
  def _ParseTests(path, version, webgl2_only, folder_min_version):
    test_paths = []
    current_dir = os.path.dirname(path)
    full_path = os.path.normpath(os.path.join(conformance_path, path))
    webgl_version = int(version.split('.')[0])

    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified ' +
        'does not exist: ' + full_path)

    with open(full_path, 'r') as f:
      for line in f:
        line = line.strip()

        if not line:
          continue

        if line.startswith('//') or line.startswith('#'):
          continue

        line_tokens = line.split(' ')
        test_name = line_tokens[-1]

        i = 0
        min_version = None
        while i < len(line_tokens):
          token = line_tokens[i]
          if token == '--min-version':
            i += 1
            min_version = line_tokens[i]
          i += 1

        min_version_to_compare = min_version or folder_min_version

        if (min_version_to_compare and
            _CompareVersion(version, min_version_to_compare) < 0):
          continue

        if (webgl2_only and not '.txt' in test_name and
            (not min_version_to_compare or
             not min_version_to_compare.startswith('2'))):
          continue

        if '.txt' in test_name:
          include_path = os.path.join(current_dir, test_name)
          # We only check min-version >= 2.0.0 for the top level list.
          test_paths += WebglConformance._ParseTests(
              include_path, version, webgl2_only, min_version_to_compare)
        else:
          test = os.path.join(current_dir, test_name)
          if webgl_version > 1:
            test += '?webglVersion=' + str(webgl_version)
          test_paths.append(test)

    return test_paths
