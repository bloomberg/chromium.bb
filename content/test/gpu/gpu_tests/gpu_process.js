// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController.setAutomationId = function(id) {};
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  };
  window.domAutomationController = domAutomationController;
})();
