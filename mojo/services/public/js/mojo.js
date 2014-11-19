// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/mojo", [
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/public/js/connection",
  "mojo/public/js/core",
  "services/js/bridge",
], function(service, connection, core, bridge) {

  function Shell() {
    this.applications_ = new Map();
  }

  Shell.prototype.connectToApplication = function(url) {
    var application = this.applications_.get(url);
    if (application)
      return application;
    application = new ServiceProvider(bridge.connectToApplication(url));
    this.applications_.set(url, application);
    return application;
  };

  Shell.prototype.connectToService = function (url, service, client) {
    return this.connectToApplication(url).connectToService(service, client);
  };

  Shell.prototype.close = function() {
    shell().applications_.forEach(function(application, url) {
      application.close();
    });
    shell().applications_.clear();
  };

  var shellValue = null;

  function shell() {
    if (!shellValue)
      shellValue = new Shell();
    return shellValue;
  }

  var requestorValue = null;

  function requestor() {
    if (!requestorValue) {
      var handle = bridge.requestorMessagePipeHandle();
      requestorValue = handle && new ServiceProvider(handle);
    }
    return requestorValue;
  }

  function connectToServiceImpl(serviceName, serviceHandle) {
    var provider = this.providers_.get(serviceName);
    if (!provider) {
      this.pendingRequests_.set(serviceName, serviceHandle);
      return;
    }

    var serviceConnection = new connection.Connection(
      serviceHandle,
      provider.service.delegatingStubClass,
      provider.service.client && provider.service.client.proxyClass);

    serviceConnection.local.connection$ = serviceConnection;
    serviceConnection.local.delegate$ =
      new provider.factory(serviceConnection.remote);

    provider.connections.push(serviceConnection);
  }

  function ServiceProvider(messagePipeHandle) {
    // TODO(hansmuller): if messagePipeHandle is null, throw an exception.
    this.connections_ = new Map();
    this.providers_ = new Map();
    this.pendingRequests_ = new Map();
    this.connection_ = null;
    this.handle_ = messagePipeHandle;
    this.connection_ =  new connection.Connection(
      this.handle_,
      service.ServiceProvider.client.delegatingStubClass,
      service.ServiceProvider.proxyClass);
    this.connection_.local.delegate$ = {
      connectToService: connectToServiceImpl.bind(this)
    };
  }

  ServiceProvider.prototype.provideService = function(service, factory) {
    // TODO(hansmuller): if !factory, remove provider and close its connections.
    // TODO(hansmuller): if this.connection_ is null, throw an error.
    var provider = {
      service: service,
      factory: factory,
      connections: [],
    };
    this.providers_.set(service.name, provider);

    if (this.pendingRequests_.has(service.name)) {
      connectToServiceImpl(service.name, pendingRequests_.get(service.name));
      pendingRequests_.delete(service.name);
    }

    return this;
  };

  ServiceProvider.prototype.connectToService = function(service, client) {
    // TODO(hansmuler): if service.name isn't defined, throw an error.
    // TODO(hansmuller): if this.connection_ is null, throw an error.
    var serviceConnection = this.connections_.get(service.name);
    if (serviceConnection)
      return serviceConnection.remote;

    var pipe = core.createMessagePipe();
    this.connection_.remote.connectToService(service.name, pipe.handle1);
    var clientClass = client && service.client.delegatingStubClass;
    var serviceConnection =
      new connection.Connection(pipe.handle0, clientClass, service.proxyClass);
    if (serviceConnection.local)
      serviceConnection.local.delegate$ = client;

    this.connections_.set(service.name, serviceConnection);
    return serviceConnection.remote;
  };

  ServiceProvider.prototype.close = function() {
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

      shell().applications_.forEach(function(application, url) {
        if (application === this)
          shell().applications_.delete(url);
      }, this);
    }
  };

  function quit() {
    if (requestorValue)
      requestor().close();
    if (shellValue)
      shell().close();
    bridge.quit();
  }

  var exports = {};
  exports.requestor = requestor;
  exports.shell = shell;
  exports.quit = quit;
  return exports;
});
