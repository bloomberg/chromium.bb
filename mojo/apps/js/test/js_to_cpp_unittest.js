// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/apps/js/test/js_to_cpp_unittest", [
    'mojo/public/js/bindings/connection',
    "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom",
], function(connector, provider) {
  var connection;

  function ProviderClientConnection(provider) {
    this.provider_ = provider;
    provider.echoString("message");
  }

  ProviderClientConnection.prototype =
    Object.create(provider.ProviderClientStub.prototype);

  return function(handle) {
    connection = new connector.Connection(handle, ProviderClientConnection,
                                          provider.ProviderProxy);
  };
});
