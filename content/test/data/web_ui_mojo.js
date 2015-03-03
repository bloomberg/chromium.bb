// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('main', [
    'mojo/public/js/router',
    'content/test/data/web_ui_test_mojo_bindings.mojom',
    'content/public/renderer/service_provider',
], function (router, bindings, serviceProvider) {
  var browserTarget;

  return function() {
    browserTarget = new bindings.BrowserTarget.proxyClass(
        new router.Router(
            serviceProvider.connectToService(bindings.BrowserTarget.name)));

    browserTarget.start().then(function() {
      browserTarget.stop();
    });
  };
});
