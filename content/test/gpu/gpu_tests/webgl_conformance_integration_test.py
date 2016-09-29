# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import webgl_conformance
from gpu_tests import webgl_conformance_expectations
from gpu_tests import webgl2_conformance_expectations

conformance_relcomps = (
  'third_party', 'webgl', 'src', 'sdk', 'tests')

extensions_relcomps = (
    'content', 'test', 'data', 'gpu')

conformance_relpath = os.path.join(*conformance_relcomps)
extensions_relpath = os.path.join(*extensions_relcomps)

# These URL prefixes are needed because having more than one static
# server dir is causing the base server directory to be moved up the
# directory hierarchy.
url_prefixes_to_trim = [
  '/'.join(conformance_relcomps) + '/',
  '/'.join(extensions_relcomps) + '/'
]

extension_harness_additional_script = r"""
  window.onload = function() { window._loaded = true; }
"""

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
    #
    # Conformance tests
    #
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
      yield (name,
             os.path.join(conformance_relpath, test_path),
             ('_RunConformanceTest'))

    #
    # Extension tests
    #
    extension_tests = cls._GetExtensionList()
    # Coverage test.
    yield('WebglExtension_TestCoverage',
          os.path.join(extensions_relpath, 'webgl_extension_test.html'),
          ('_RunExtensionCoverageTest',
           extension_tests,
           cls._webgl_version))
    # Individual extension tests.
    for extension in extension_tests:
      yield('WebglExtension_%s' % extension,
            os.path.join(extensions_relpath, 'webgl_extension_test.html'),
            ('_RunExtensionTest',
             extension,
             cls._webgl_version))

  @classmethod
  def _GetExtensionList(cls):
    if cls._webgl_version == 1:
      return [
        'ANGLE_instanced_arrays',
        'EXT_blend_minmax',
        'EXT_disjoint_timer_query',
        'EXT_frag_depth',
        'EXT_shader_texture_lod',
        'EXT_sRGB',
        'EXT_texture_filter_anisotropic',
        'OES_element_index_uint',
        'OES_standard_derivatives',
        'OES_texture_float',
        'OES_texture_float_linear',
        'OES_texture_half_float',
        'OES_texture_half_float_linear',
        'OES_vertex_array_object',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_atc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_depth_texture',
        'WEBGL_draw_buffers',
        'WEBGL_lose_context',
      ]
    else:
      return [
        'EXT_color_buffer_float',
        'EXT_disjoint_timer_query',
        'EXT_texture_filter_anisotropic',
        'OES_texture_float_linear',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_atc',
        'WEBGL_compressed_texture_es3_0',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_lose_context',
      ]

  def RunActualGpuTest(self, test_path, *args):
    # This indirection allows these tests to trampoline through
    # _RunGpuTest.
    test_name = args[0]
    getattr(self, test_name)(test_path, *args[1:])

  def _NavigateTo(self, test_path, harness_script):
    url = self.UrlOfStaticFilePath(test_path)
    self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)

  def _CheckTestCompletion(self):
    self.tab.action_runner.WaitForJavaScriptCondition(
        'webglTestHarness._finished', timeout_in_seconds=300)
    if not webgl_conformance._DidWebGLTestSucceed(self.tab):
      self.fail(webgl_conformance._WebGLTestMessages(self.tab))

  def _RunConformanceTest(self, test_path, *args):
    self._NavigateTo(test_path, webgl_conformance.conformance_harness_script)
    self._CheckTestCompletion()


  def _GetExtensionHarnessScript(self):
    return (webgl_conformance.conformance_harness_script +
            extension_harness_additional_script)

  def _RunExtensionCoverageTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout_in_seconds=300)
    extension_list = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    extension_list_string = "["
    for extension in extension_list:
      extension_list_string = extension_list_string + extension + ", "
    extension_list_string = extension_list_string + "]"
    self.tab.action_runner.EvaluateJavaScript(
      'checkSupportedExtensions("%s", "%s")' % (
        extension_list_string, context_type))
    self._CheckTestCompletion()

  def _RunExtensionTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout_in_seconds=300)
    extension = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    self.tab.action_runner.EvaluateJavaScript(
      'checkExtension("%s", "%s")' % (extension, context_type))
    self._CheckTestCompletion()

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
          webgl_conformance.conformance_path, url_prefixes=url_prefixes_to_trim)
    else:
      return webgl2_conformance_expectations.WebGL2ConformanceExpectations(
          webgl_conformance.conformance_path, url_prefixes=url_prefixes_to_trim)

  @classmethod
  def setUpClass(cls):
    super(cls, WebGLConformanceIntegrationTest).setUpClass()
    cls.CustomizeOptions()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()
    # By setting multiple server directories, the root of the server
    # implicitly becomes the common base directory, i.e., the Chromium
    # src dir, and all URLs have to be specified relative to that.
    cls.SetStaticServerDirs([
      os.path.join(path_util.GetChromiumSrcDir(), conformance_relpath),
      os.path.join(path_util.GetChromiumSrcDir(), extensions_relpath)])
