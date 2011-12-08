// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testPluginWorks() {
  var plug = document.getElementById("plugin");
  var success = false;
  if (plug && plug.testInvokeDefault) {
    plug.testInvokeDefault(function(str) {
      success = true;
    });
  }
  window.domAutomationController.send(success);
}
