// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/apps/js/bootstrap",
    "mojo/public/bindings/js/connector",
    "mojom/hello_world_service",
], function(console, bootstrap, connector, hello) {

  function HelloWorldClientImpl() {
  }

  HelloWorldClientImpl.prototype =
      Object.create(hello.HelloWorldClientStub.prototype);

  HelloWorldClientImpl.prototype.didReceiveGreeting = function(result) {
    console.log("DidReceiveGreeting from pipe: " + result);
    connection.close();
    bootstrap.quit();
  };

  var connection = new connector.Connection(bootstrap.initialHandle,
                                            HelloWorldClientImpl,
                                            hello.HelloWorldServiceProxy);

  connection.remote.greeting("hello, world!");
});
