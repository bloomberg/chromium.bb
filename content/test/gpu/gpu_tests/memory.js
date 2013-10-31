// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (window.parent != window)  // Ignore subframes.
    return;

  // If changed, make sure the value can still be queried in memory.py.
  var MEMORY_LIMIT_MB = 256;

  var domAutomationController = {};
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    // This should wait until all effects of memory management complete.
    // We will need to wait until all
    // 1. pending commits from the main thread to the impl thread in the
    //    compositor complete (for visible compositors).
    // 2. allocations that the renderer's impl thread will make due to the
    //    compositor and WebGL are completed.
    // 3. pending GpuMemoryManager::Manage() calls to manage are made.
    // 4. renderers' OnMemoryAllocationChanged callbacks in response to
    //    manager are made.
    // Each step in this sequence can cause trigger the next (as a 1-2-3-4-1
    // cycle), so we will need to pump this cycle until it stabilizes.

    // Pump the cycle 8 times (in principle it could take an infinite number
    // of iterations to settle).

    var rafCount = 0;
    var totalRafCount = 8;

    function pumpRAF() {
      if (rafCount == totalRafCount) {
        domAutomationController._finished = true;
        return;
      }
      ++rafCount;
      window.requestAnimationFrame(pumpRAF);
    }
    pumpRAF();
  };

  window.domAutomationController = domAutomationController;

  window.addEventListener("load", function() {
    useGpuMemory(MEMORY_LIMIT_MB);
  }, false);
})();
