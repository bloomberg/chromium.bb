# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test
from telemetry.page import page
from telemetry.page import page_set
from telemetry.page import page_test
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *

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

class WebglRobustnessPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(WebglRobustnessPage, self).__init__(
      url='file://extra/lots-of-polys-example.html',
      page_set=page_set,
      base_dir=base_dir)
    self.script_to_evaluate_on_commit = robustness_harness_script

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(
      WaitAction({'javascript': 'webglTestHarness._finished'}))

class WebglRobustness(test.Test):
  test = WebglConformanceValidator

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      file_path=conformance_path,
      description='Test cases for WebGL robustness',
      user_agent_type='desktop',
      serving_dirs=[''])
    ps.AddPage(WebglRobustnessPage(ps, ps.base_dir))
    return ps
