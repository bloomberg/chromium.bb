// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/application", [
  "mojo/public/js/bindings",
  "mojo/public/js/threading",
  "mojo/services/public/js/service_provider",
  "mojo/services/public/js/shell",
], function(bindings, threading, serviceProvider, shell) {

  const ProxyBindings = bindings.ProxyBindings;
  const ServiceProvider = serviceProvider.ServiceProvider;
  const Shell = shell.Shell;

  class Application {
    constructor(shellHandle, url) {
      this.url = url;
      this.serviceProviders = [];
      this.exposedServiceProviders = [];
      this.shellHandle_ = shellHandle;
      this.shell = new Shell(shellHandle, {
        initialize: this.initialize.bind(this),
        acceptConnection: this.doAcceptConnection.bind(this),
      });
    }

    initialize(args) {
    }

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
      var serviceProvider = new ServiceProvider(exposedServicesProxy);
      this.serviceProviders.push(serviceProvider);

      // Then associate incoming calls with the serviceProvider.
      ProxyBindings(servicesRequest).setLocalDelegate(serviceProvider);

      this.acceptConnection(requestorUrl, serviceProvider);
    }

    acceptConnection(requestorUrl, serviceProvider) {
    }

    quit() {
      this.serviceProviders.forEach(function(sp) {
        sp.close();
      });
      this.shell.close();
      threading.quit();
    }
  }

  var exports = {};
  exports.Application = Application;
  return exports;
});
