// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/service_provider", [
  "mojo/public/js/bindings",
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/public/js/connection",
], function(bindings, serviceProviderMojom, connection) {

  const ProxyBindings = bindings.ProxyBindings;
  const StubBindings = bindings.StubBindings;
  const ServiceProviderInterface = serviceProviderMojom.ServiceProvider;

  function checkServiceProvider(sp) {
    if (!sp.providers_)
      throw new Error("Service was closed");
  }

  class ServiceProvider {
    constructor(service) {
      if (!(service instanceof ServiceProviderInterface.proxyClass))
        throw new Error("service must be a ServiceProvider proxy");
      this.proxy = service;
      ProxyBindings(this.proxy).setLocalDelegate(this);
      this.providers_ = new Map(); // serviceName => see provideService() below
      this.pendingRequests_ = new Map(); // serviceName => serviceHandle
    }

    // Incoming requests
    connectToService(serviceName, serviceHandle) {
      if (!this.providers_) // We're closed.
        return;

      var provider = this.providers_.get(serviceName);
      if (!provider) {
        this.pendingRequests_.set(serviceName, serviceHandle);
        return;
      }
      var proxy = connection.bindProxyHandle(
          serviceHandle, provider.service, provider.service.client);
      if (ProxyBindings(proxy).local)
        ProxyBindings(proxy).setLocalDelegate(new provider.factory(proxy));
      provider.connections.push(ProxyBindings(proxy).connection);
    }

    provideService(service, factory) {
      checkServiceProvider(this);

      var provider = {
        service: service, // A JS bindings interface object.
        factory: factory, // factory(clientProxy) => interface implemntation
        connections: [],
      };
      this.providers_.set(service.name, provider);

      if (this.pendingRequests_.has(service.name)) {
        this.connectToService(service.name, pendingRequests_.get(service.name));
        pendingRequests_.delete(service.name);
      }
      return this;
    }

    // Outgoing requests
    requestService(interfaceObject, clientImpl) {
      checkServiceProvider(this);
      if (!interfaceObject.name)
        throw new Error("Invalid service parameter");
      if (!clientImpl && interfaceObject.client)
        throw new Error("Client implementation must be provided");

      var remoteProxy;
      var clientFactory = function(x) {remoteProxy = x; return clientImpl;};
      var messagePipeHandle = connection.bindProxyClient(
          clientFactory, interfaceObject.client, interfaceObject);
      this.proxy.connectToService(interfaceObject.name, messagePipeHandle);
      return remoteProxy;
    };

    close() {
      this.providers_ = null;
      this.pendingRequests_ = null;
    }
  }

  var exports = {};
  exports.ServiceProvider = ServiceProvider;
  return exports;
});
