// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/shell", [
  "mojo/public/js/core",
  "mojo/public/js/connection",
  "mojo/public/interfaces/application/shell.mojom",
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/services/public/js/service_provider"
], function(core,
            connection,
            shellMojom,
            serviceProviderMojom,
            serviceProvider) {

  const ServiceProvider = serviceProvider.ServiceProvider;
  const ServiceProviderInterface = serviceProviderMojom.ServiceProvider;
  const ShellInterface = shellMojom.Shell;

  class Shell {
    constructor(shellHandle, app) {
      this.shellHandle = shellHandle;
      this.proxy = connection.bindProxyHandle(
          shellHandle, ShellInterface.client, ShellInterface);
      this.proxy.local$ = app; // The app is the shell's client.
      // TODO: call this serviceProviders_
      this.applications_ = new Map();
    }

    connectToApplication(url) {
      var application = this.applications_.get(url);
      if (application)
        return application;

      var returnValue = {};
      this.proxy.connectToApplication(url, returnValue);
      application = new ServiceProvider(returnValue.remote$);
      this.applications_.set(url, application);
      return application;
    }

    connectToService(url, service, client) {
      return this.connectToApplication(url).requestService(service, client);
    };

    close() {
      this.applications_.forEach(function(application, url) {
        application.close();
      });
      this.applications_.clear();
      core.close(this.shellHandle);
    }
  }

  var exports = {};
  exports.Shell = Shell;
  return exports;
});
