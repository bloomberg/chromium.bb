# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test
from telemetry.page import page_set
from webgl_conformance import WebglConformanceValidator
from webgl_conformance import conformance_harness_script
from webgl_conformance import conformance_path


robustness_harness_script = conformance_harness_script + r"""
  var robustnessTestHarness = {};
  robustnessTestHarness._contextLost = false;
  robustnessTestHarness.initialize = function() {
    var canvas = document.getElementById('example');
    canvas.addEventListener('webglcontextlost', function() {
      robustnessTestHarness._contextLost = true;
    });
  }
  robustnessTestHarness.runTestLoop = function() {
    // Run the test in a loop until the context is lost.
    main();
    if (!robustnessTestHarness._contextLost)
      window.requestAnimationFrame(robustnessTestHarness.runTestLoop);
    else
      robustnessTestHarness.notifyFinished();
  }
  robustnessTestHarness.notifyFinished = function() {
    // The test may fail in unpredictable ways depending on when the context is
    // lost. We ignore such errors and only require that the browser doesn't
    // crash.
    webglTestHarness._allTestSucceeded = true;
    // Notify test completion after a delay to make sure the browser is able to
    // recover from the lost context.
    setTimeout(webglTestHarness.notifyFinished, 3000);
  }

  window.confirm = function() {
    robustnessTestHarness.initialize();
    robustnessTestHarness.runTestLoop();
    return false;
  }
  window.webglRobustnessTestHarness = robustnessTestHarness;
"""


class WebglRobustness(test.Test):
  test = WebglConformanceValidator

  def CreatePageSet(self, options):
    page_set_dict = {
      'description': 'Test cases for WebGL robustness',
      'user_agent_type': 'desktop',
      'serving_dirs': [''],
      'pages': [
        {
          'url': 'file://extra/lots-of-polys-example.html',
          'script_to_evaluate_on_commit': robustness_harness_script,
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait', 'javascript': 'webglTestHarness._finished' }
          ]
        }
      ]
    }
    return page_set.PageSet.FromDict(page_set_dict, conformance_path)
