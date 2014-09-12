// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This trivial app just loads "cnn.com" using the Mojo Network and URLLoader
// services and then prints a brief summary of the response. It's intended to
// be run using the mojo_js content handler and assumes that this source file
// is specified as a URL. For example:
//
//   (cd YOUR_DIR/mojo/apps/js; python -m SimpleHTTPServer ) &
//   mojo_shell --content-handlers=application/javascript,mojo://mojo_js \
//       http://localhost:8000/test.js

define("test", [
  "mojo/public/js/bindings/core",
  "mojo/public/js/bindings/connection",
  "mojo/services/public/interfaces/network/network_service.mojom",
  "mojo/services/public/interfaces/network/url_loader.mojom",
  "mojo",
  "console"
], function(core, connection, network, loader, mojo, console) {

  function NetworkServiceImpl(remote) { }
  NetworkServiceImpl.prototype =
    Object.create(network.NetworkServiceStub.prototype);

  function URLLoaderImpl(remote) { }
  URLLoaderImpl.prototype =
    Object.create(loader.URLLoaderStub.prototype);

  var networkServiceHandle = mojo.connectToService(
      "mojo:mojo_network_service", "mojo::NetworkService");
  var networkConnection = new connection.Connection(
      networkServiceHandle, NetworkServiceImpl, network.NetworkServiceProxy);

  var urlLoaderPipe = new core.createMessagePipe();
  networkConnection.remote.createURLLoader(urlLoaderPipe.handle1);
  var urlLoaderConnection = new connection.Connection(
      urlLoaderPipe.handle0, URLLoaderImpl, loader.URLLoaderProxy);

  var urlRequest = new loader.URLRequest();
  urlRequest.url = "http://www.cnn.com";
  urlRequest.method = "GET";
  urlRequest.auto_follow_redirects = true;
  var urlRequestPromise = urlLoaderConnection.remote.start(urlRequest);
  urlRequestPromise.then(
    function(result) {
      var body = core.readData(result.response.body,
                               core.READ_DATA_FLAG_ALL_OR_NONE);
      if (body.result == core.RESULT_OK)
        console.log("body.buffer.byteLength=" + body.buffer.byteLength);
      else
        console.log("core.readData() failed err=" + body.result);
      for(var key in result.response)
        console.log(key + " => " + result.response[key]);
      mojo.quit();
    },
    function(error) {
      console.log("FAIL " + error.toString());
      mojo.quit();
    });

});
