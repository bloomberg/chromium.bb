// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function callTestInvokeDefault(plugin) {
  if (plugin.testInvokeDefault) {
    plugin.testInvokeDefault(function(str) {
      window.domAutomationController.send(true);
    });
    return;
  }

  // Try again in 100 ms.
  window.setTimeout(callTestInvokeDefault.bind(null, plugin), 100);
}

function testPluginWorks() {
  if (!navigator.mimeTypes['application/x-extension-test']) {
    window.domAutomationController.send(false);
    return;
  }

  var plug = document.getElementById("plugin");
  if (!plug) {
    window.domAutomationController.send(false);
    return;
  }
  callTestInvokeDefault(plug);
}
