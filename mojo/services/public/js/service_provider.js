// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/service_provider", [
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/public/js/connection",
  "mojo/public/js/core",
], function(spInterfaceModule, connectionModule, coreModule) {

  // Implementation of the Mojo ServiceProvider interface.
  function connectToServiceImpl(serviceName, serviceHandle) {
    var provider = this.providers_.get(serviceName);
    if (!provider) {
      this.pendingRequests_.set(serviceName, serviceHandle);
      return;
    }

    var serviceConnection = new connectionModule.Connection(
      serviceHandle,
      provider.service.stubClass,
      provider.service.client && provider.service.client.proxyClass);

    serviceConnection.local.delegate$ =
      new provider.factory(serviceConnection.remote);

    provider.connections.push(serviceConnection);
  }

  function checkServiceProvider(sp) {
    if (!sp.connections_)
      throw new Error("Service was closed");
  }

  class ServiceProvider {
    constructor(service) {
      if (!(service instanceof spInterfaceModule.ServiceProvider.proxyClass))
        throw new Error("service must be a ServiceProvider proxy");

      service.client$ = {
        connectToService: connectToServiceImpl.bind(this)
      };

      this.connections_ = new Map();
      this.providers_ = new Map();
      this.pendingRequests_ = new Map();
      this.connection_ = null;
      this.connection_ = service.getConnection$();
    }

    provideService(service, factory) {
      // TODO(hansmuller): if !factory, remove provider and close its
      // connections.
      checkServiceProvider(this);

      var provider = {
        service: service, // A JS bindings interface object.
        factory: factory, // factory(clientProxy) => interface implemntation.
        connections: [],
      };
      this.providers_.set(service.name, provider);

      if (this.pendingRequests_.has(service.name)) {
        connectToServiceImpl(service.name, pendingRequests_.get(service.name));
        pendingRequests_.delete(service.name);
      }

      return this;
    }

    connectToService(service, client) {
      checkServiceProvider(this);
      if (!service.name)
        throw new Error("Invalid service parameter");

      var serviceConnection = this.connections_.get(service.name);
      if (serviceConnection)
        return serviceConnection.remote;

      var pipe = coreModule.createMessagePipe();
      this.connection_.remote.connectToService(service.name, pipe.handle1);
      var clientClass = client && service.client.stubClass;
      var serviceConnection = new connectionModule.Connection(
        pipe.handle0, clientClass, service.proxyClass);
      if (serviceConnection.local)
        serviceConnection.local.delegate$ = client;

      this.connections_.set(service.name, serviceConnection);
      return serviceConnection.remote;
    };

    close() {
      if (!this.connection_)
        return;

      try {
        // Outgoing connections
        this.connections_.forEach(function(connection, serviceName) {
          connection.close();
        });
        // Incoming connections
        this.providers_.forEach(function(provider, serviceName) {
          provider.connections.forEach(function(connection) {
            connection.close();
          });
        });
        this.connection_.close();
      } finally {
        this.connections_ = null;
        this.providers_ = null;
        this.pendingRequests_ = null;
        this.connection_ = null;
        this.handle_ = null;
      }
    }
  }

  var exports = {};
  exports.ServiceProvider = ServiceProvider;
  return exports;
});
