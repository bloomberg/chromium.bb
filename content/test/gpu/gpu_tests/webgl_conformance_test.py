# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import json

from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.core import util

src_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
conformance_path = os.path.join(src_path, 'third_party', 'webgl_conformance')

conformance_harness_script = r"""
  var testHarness = {};
  testHarness._allTestSucceeded = true;
  testHarness._messages = '';
  testHarness._failures = 0;
  testHarness._finished = false;

  testHarness.reportResults = function(success, msg) {
    testHarness._allTestSucceeded = testHarness._allTestSucceeded && !!success;
    if(!success) {
      testHarness._failures++;
      if(msg) {
        testHarness._messages += msg + "\n";
      }
    }
  };
  testHarness.notifyFinished = function() {
    testHarness._finished = true;
  };
  testHarness.navigateToPage = function(src) {
    var testFrame = document.getElementById("test-frame");
    testFrame.src = src;
  };

  window.webglTestHarness = testHarness;
  window.parent.webglTestHarness = testHarness;
  console.log("Harness injected.");
"""

def _DidWebGLTestSucceed(tab):
  return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded')

def _WebGLTestMessages(tab):
  return tab.EvaluateJavaScript('webglTestHarness._messages')

class WebGLConformanceTest(page_test.PageTest):
  def __init__(self):
    super(WebGLConformanceTest, self).__init__('ValidatePage')

  def CreatePageSet(self, options):
    tests = WebGLConformanceTest._ParseTests('00_test_list.txt')

    page_set_dict = {
      'description': 'Executes WebGL conformance tests',
      'user_agent_type': 'desktop',
      'serving_dirs': [
        '../../../../third_party/webgl_conformance'
      ],
      'pages': []
    }

    pages = page_set_dict['pages']

    for test in tests:
      pages.append({
        'url': 'file:///../../../../third_party/webgl_conformance/' + test,
        'script_to_evaluate_on_commit': conformance_harness_script,
        'wait_for_javascript_expression': 'webglTestHarness._finished'
      })

    return page_set.PageSet.FromDict(page_set_dict, __file__)

  def ValidatePage(self, page, tab, results):
    if _DidWebGLTestSucceed(tab):
      results.AddSuccess(page)
    else:
      results.AddFailure(page, _WebGLTestMessages(tab), None)

  @staticmethod
  def _ParseTests(path, version = None):
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
        while i < len(line_tokens):
          token = line_tokens[i]
          if token == '--min-version':
            i += 1
            min_version = line_tokens[i]
          i += 1

        if version and version < min_version:
          continue

        test_name = line_tokens[-1]

        if '.txt' in test_name:
          include_path = os.path.join(current_dir, test_name)
          test_paths += WebGLConformanceTest._ParseTests(
            include_path, version)
        else:
          test = os.path.join(current_dir, test_name)
          test_paths.append(test)

    return test_paths
