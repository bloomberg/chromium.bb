// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * This URL is defined in content/public/test/test_mojo_app.cc. It identifies
 * content_shell's out-of-process Mojo test app.
 */
var TEST_APP_URL = 'system:content_mojo_test';


define('main', [
  'mojo/application/public/interfaces/shell.mojom',
  'mojo/public/js/core',
  'mojo/public/js/router',
  'mojo/services/network/public/interfaces/url_loader.mojom',
  'content/public/renderer/service_provider',
  'content/public/test/test_mojo_service.mojom',
], function(shellMojom, core, router, urlMojom, serviceRegistry, testMojom) {

  var connectToService = function(serviceProvider, iface) {
    var pipe = core.createMessagePipe();
    var service = new iface.proxyClass(new router.Router(pipe.handle0));
    serviceProvider.connectToService(iface.name, pipe.handle1);
    return service;
  };

  return function() {
    domAutomationController.setAutomationId(0);
    var shellPipe = serviceRegistry.connectToService(shellMojom.Shell.name);
    var shell = new shellMojom.Shell.proxyClass(new router.Router(shellPipe));
    var urlRequest = new urlMojom.URLRequest({ url: TEST_APP_URL });
    shell.connectToApplication(urlRequest,
        function (services) {
          var test = connectToService(services, testMojom.TestMojoService);
          test.getRequestorURL().then(function(response) {
            // The requestor URL seen by the app should be localhost because the
            // connection comes from this script hosted on the test server.
            domAutomationController.send(response.url == 'http://127.0.0.1/');
          });
        },
        function (exposedServices) {});
  };
});
