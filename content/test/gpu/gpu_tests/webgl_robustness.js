// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
  if (window.parent != window)  // Ignore subframes.
    return;

  var robustnessTestHarness = {};
  robustnessTestHarness._contextLost = false;
  robustnessTestHarness.initialize = function() {
    var canvas = document.getElementById('example');
    canvas.addEventListener('webglcontextlost', function() {
      robustnessTestHarness._contextLost = true;
    });
  };
  robustnessTestHarness.runTestLoop = function() {
    // Run the test in a loop until the context is lost.
    main();
    if (!robustnessTestHarness._contextLost)
      window.requestAnimationFrame(robustnessTestHarness.runTestLoop);
    else
      robustnessTestHarness.notifyFinished();
  };
  robustnessTestHarness.notifyFinished = function() {
    // The test may fail in unpredictable ways depending on when the context is
    // lost. We ignore such errors and only require that the browser doesn't
    // crash.
    webglTestHarness._allTestSucceeded = true;
    // Notify test completion after a delay to make sure the browser is able to
    // recover from the lost context.
    setTimeout(webglTestHarness.notifyFinished, 3000);
  };

  window.confirm = function() {
    robustnessTestHarness.initialize();
    robustnessTestHarness.runTestLoop();
    return false;
  };
  window.webglRobustnessTestHarness = robustnessTestHarness;
})();
