# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import optparse
import os
import sys

import webgl_conformance_expectations

from telemetry import test as test_module
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test

conformance_path = os.path.join(
    util.GetChromiumSrcDir(),
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
"""

def _DidWebGLTestSucceed(tab):
  return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded')

def _WebGLTestMessages(tab):
  return tab.EvaluateJavaScript('webglTestHarness._messages')

class WebglConformanceValidator(page_test.PageTest):
  def __init__(self):
    super(WebglConformanceValidator, self).__init__('ValidatePage',
        attempts=1,
        max_errors=10)

  def ValidatePage(self, page, tab, results):
    if not _DidWebGLTestSucceed(tab):
      raise page_test.Failure(_WebGLTestMessages(tab))

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--disable-gesture-requirement-for-media-playback',
        '--disable-domain-blocking-for-3d-apis',
        '--disable-gpu-process-crash-limit'
    ])


class WebglConformance(test_module.Test):
  """Conformance with Khronos WebGL Conformance Tests"""
  test = WebglConformanceValidator

  @classmethod
  def AddTestCommandLineArgs(cls, group):
    group.add_option('--webgl-conformance-version',
        help='Version of the WebGL conformance tests to run.',
        default='1.0.1')

  def CreatePageSet(self, options):
    tests = self._ParseTests('00_test_list.txt',
        options.webgl_conformance_version)

    page_set_dict = {
      'description': 'Executes WebGL conformance tests',
      'user_agent_type': 'desktop',
      'serving_dirs': [ '' ],
      'pages': []
    }

    pages = page_set_dict['pages']

    for test in tests:
      pages.append({
        'name': 'WebglConformance.%s' %
            test.replace('/', '_').replace('-', '_').
            replace('\\', '_').rpartition('.')[0].replace('.', '_'),
        'url': 'file://' + test,
        'script_to_evaluate_on_commit': conformance_harness_script,
        'navigate_steps': [
          {'action': 'navigate'},
          {
            'action': 'wait',
            'javascript': 'webglTestHarness._finished',
            'timeout': 120
          }
        ]
      })

    return page_set.PageSet.FromDict(page_set_dict, conformance_path)

  def CreateExpectations(self, page_set):
    return webgl_conformance_expectations.WebGLConformanceExpectations()

  @staticmethod
  def _ParseTests(path, version=None):
    test_paths = []
    current_dir = os.path.dirname(path)
    full_path = os.path.normpath(os.path.join(conformance_path, path))

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

        i = 0
        min_version = None
        while i < len(line_tokens):
          token = line_tokens[i]
          if token == '--min-version':
            i += 1
            min_version = line_tokens[i]
          i += 1

        if version and min_version and version < min_version:
          continue

        test_name = line_tokens[-1]

        if '.txt' in test_name:
          include_path = os.path.join(current_dir, test_name)
          test_paths += WebglConformance._ParseTests(
            include_path, version)
        else:
          test = os.path.join(current_dir, test_name)
          test_paths.append(test)

    return test_paths
