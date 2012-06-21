// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Bluetooth API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;

// Use custom bindings to create an undocumented event listener that will
// receive events about device discovery and call the event listener that was
// provided with the request to begin discovery.
chromeHidden.registerCustomHook('experimental.bluetooth', function(api) {
  var apiFunctions = api.apiFunctions;

  chromeHidden.bluetooth = {};
  chromeHidden.bluetooth.handler = null;
  chromeHidden.bluetooth.onDeviceDiscovered =
      new chrome.Event("experimental.bluetooth.onDeviceDiscovered");

  function deviceDiscoveredListener(device) {
    if (chromeHidden.bluetooth.handler != null)
      chromeHidden.bluetooth.handler(device);
  }
  chromeHidden.bluetooth.onDeviceDiscovered.addListener(
      deviceDiscoveredListener);

  apiFunctions.setHandleRequest('startDiscovery', function() {
    var args = arguments;
    if (args.length > 0 && args[0] && args[0].deviceCallback) {
      chromeHidden.bluetooth.handler = args[0].deviceCallback;
    }
    sendRequest(this.name, args, this.definition.parameters);
  });
  apiFunctions.setHandleRequest('stopDiscovery', function() {
    chromeHidden.bluetooth.handler = null;
    sendRequest(this.name, arguments, this.definition.parameters);
  });
});
