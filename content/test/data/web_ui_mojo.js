// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('main', [
    'mojo/public/js/bindings/connection',
    'content/test/data/web_ui_test_mojo_bindings.mojom',
    'content/public/renderer/service_provider',
], function (connection, bindings, serviceProvider) {
  var retainedConnection;

  function RendererTargetTest(bindings) {
    this.bindings_ = bindings;
  }

  // TODO(aa): It is a bummer to need this stub object in JavaScript. We should
  // have a 'client' object that contains both the sending and receiving bits of
  // the client side of the interface. Since JS is loosely typed, we do not need
  // a separate base class to inherit from to receive callbacks.
  RendererTargetTest.prototype =
      Object.create(bindings.RendererTargetStub.prototype);

  RendererTargetTest.prototype.ping = function () {
    this.bindings_.pingResponse();
  };

  return function() {
    retainedConnection = new connection.Connection(
        // TODO(sammc): Avoid using NAME_ directly.
        serviceProvider.connectToService(bindings.BrowserTargetProxy.NAME_),
        RendererTargetTest,
        bindings.BrowserTargetProxy);
  };
});
