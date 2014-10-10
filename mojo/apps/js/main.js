// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This trivial app just loads "cnn.com" using the Mojo Network and URLLoader
// services and then prints a brief summary of the response.
//
// To run it using mojo_js_standalone (don't forget the quotes):
//   mojo_shell 'mojo://mojo_js_standalone THIS_DIR/main.js'
//
// To run it using mojo_js_content handler this file must be specified as
// a URL. For example:
//   (cd YOUR_DIR/mojo/apps/js; python -m SimpleHTTPServer ) &
//   mojo_shell \
//    --content-handlers=application/javascript,mojo://mojo_js_content_handler \
//    http://localhost:8000/test.js

define("test", [
  "mojo/apps/js/mojo",
  "mojo/public/js/bindings/core",
  "mojo/public/js/bindings/connection",
  "mojo/public/js/bindings/support",
  "mojo/services/public/interfaces/network/network_service.mojom",
  "mojo/services/public/interfaces/network/url_loader.mojom",
  "console"
], function(mojo, core, connection, support, net, loader, console) {

  var networkService = mojo.connectToService(
      "mojo:mojo_network_service", net.NetworkService);

  var urlLoaderPipe = core.createMessagePipe();
  networkService.createURLLoader(urlLoaderPipe.handle1);
  var urlLoaderConnection = new connection.Connection(
      urlLoaderPipe.handle0,
      function(){},
      loader.URLLoader.proxyClass);

  var urlRequest = new loader.URLRequest({
    url: "http://www.cnn.com",
    method: "GET",
    auto_follow_redirects: true
  });

  var urlRequestPromise = urlLoaderConnection.remote.start(urlRequest);
  urlRequestPromise.then(function(result) {
    for(var key in result.response)
      console.log(key + " => " + result.response[key]);
    var drainDataPromise = core.drainData(result.response.body);
    drainDataPromise.then(function(result) {
      console.log("read " + result.buffer.byteLength + " bytes");
    }).then(function() {
      mojo.quit();
    });
  });
});
