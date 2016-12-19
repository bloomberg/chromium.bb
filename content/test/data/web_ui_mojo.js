// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('main', [
    'content/public/renderer/frame_interfaces',
    'content/test/data/web_ui_test_mojo_bindings.mojom',
    'mojo/public/js/bindings',
], function (interfaceProvider, webUITest, bindings) {
  var browserTarget;

  return function() {
    browserTarget = new webUITest.BrowserTargetPtr(
        interfaceProvider.getInterface(webUITest.BrowserTarget.name));

    browserTarget.start().then(function() {
      browserTarget.stop();
    });
  };
});
