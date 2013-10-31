// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (window.parent != window)  // Ignore subframes.
    return;

  var domAutomationController = {};

  domAutomationController._succeeded = false;

  domAutomationController.setAutomationId = function(id) {};

  domAutomationController.send = function(msg) {
    msg = msg.toLowerCase();
    if (msg == "success")
      domAutomationController._succeeded = true;
    else
      domAutomationController._succeeded = false;
  };

  window.domAutomationController = domAutomationController;
})();
