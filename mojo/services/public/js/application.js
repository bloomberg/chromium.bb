// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/application", [
  "mojo/public/js/bindings",
  "mojo/public/js/core",
  "mojo/public/js/connection",
  "mojo/public/js/threading",
  "mojo/public/interfaces/application/application.mojom",
  "mojo/services/public/js/service_provider",
  "mojo/services/public/js/shell",
], function(bindings, core, connection, threading, applicationMojom, serviceProvider, shell) {

  const ApplicationInterface = applicationMojom.Application;
  const ProxyBindings = bindings.ProxyBindings;
  const ServiceProvider = serviceProvider.ServiceProvider;
  const Shell = shell.Shell;

  class Application {
    constructor(appRequestHandle, url) {
      this.url = url;
      this.serviceProviders = [];
      this.exposedServiceProviders = [];
      this.appRequestHandle_ = appRequestHandle;
      this.appStub_ =
          connection.bindHandleToStub(appRequestHandle, ApplicationInterface);
      bindings.StubBindings(this.appStub_).delegate = {
          initialize: this.doInitialize.bind(this),
          acceptConnection: this.doAcceptConnection.bind(this),
      };
    }

    doInitialize(shellProxy, args) {
      this.shellProxy_ = shellProxy;
      this.shell = new Shell(shellProxy);
      this.initialize(args);
    }

    initialize(args) {}

    // The mojom signature of this function is:
    //   AcceptConnection(string requestor_url,
    //                    ServiceProvider&? services,
    //                    ServiceProvider? exposed_services);
    //
    // We want to bind |services| to our js implementation of ServiceProvider
    // and store |exposed_services| so we can request services of the connecting
    // application.
    doAcceptConnection(requestorUrl, servicesRequest, exposedServicesProxy) {
      // Construct a new js ServiceProvider that can make outgoing calls on
      // exposedServicesProxy.
      var serviceProvider =
          new ServiceProvider(servicesRequest, exposedServicesProxy);
      this.serviceProviders.push(serviceProvider);
      this.acceptConnection(requestorUrl, serviceProvider);
    }

    acceptConnection(requestorUrl, serviceProvider) {}

    quit() {
      this.serviceProviders.forEach(function(sp) {
        sp.close();
      });
      this.shell.close();
      core.close(this.appRequestHandle_);
      threading.quit();
    }
  }

  var exports = {};
  exports.Application = Application;
  return exports;
});
