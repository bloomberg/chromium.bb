// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/apps/js/mojo", [
  "mojo/public/js/bindings/connection",
  "mojo/apps/js/bridge",
], function(connection, mojo) {

  function connectToService(url, service, client) {
    var serviceHandle = mojo.connectToService(url, service.name);
    var clientClass = client && service.client.delegatingStubClass;
    var serviceConnection =
      new connection.Connection(serviceHandle, clientClass, service.proxyClass);
    if (serviceConnection.local)
      serviceConnection.local.delegate$ = client;
    serviceConnection.remote.connection$ = serviceConnection;
    return serviceConnection.remote;
  }

  var exports = {};
  exports.connectToService = connectToService;
  exports.quit = mojo.quit;
  return exports;
});

