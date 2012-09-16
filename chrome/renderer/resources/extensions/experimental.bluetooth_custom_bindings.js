// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Bluetooth API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var lastError = require('last_error');

// Use custom bindings to create an undocumented event listener that will
// receive events about device discovery and call the event listener that was
// provided with the request to begin discovery.
chromeHidden.registerCustomHook('experimental.bluetooth', function(api) {
  var apiFunctions = api.apiFunctions;

  chromeHidden.bluetooth = {};
  chromeHidden.bluetooth.deviceDiscoveredHandler = null;
  chromeHidden.bluetooth.onDeviceDiscovered =
      new chrome.Event("experimental.bluetooth.onDeviceDiscovered");

  function clearDeviceDiscoveredHandler() {
    chromeHidden.bluetooth.onDeviceDiscovered.removeListener(
        chromeHidden.bluetooth.deviceDiscoveredHandler);
    chromeHidden.bluetooth.deviceDiscoveredHandler = null;
  }

  apiFunctions.setHandleRequest('startDiscovery',
      function() {
        var args = arguments;
        if (args.length > 0 && args[0] && args[0].deviceCallback) {
          chromeHidden.bluetooth.deviceDiscoveredHandler =
              args[0].deviceCallback;
          chromeHidden.bluetooth.onDeviceDiscovered.addListener(
              chromeHidden.bluetooth.deviceDiscoveredHandler);
          sendRequest(this.name,
                      args,
                      this.definition.parameters,
                      {customCallback:this.customCallback});
        } else {
          if (typeof(args[args.length-1]) == "function") {
            var callback = args[args.length-1];
            lastError.set("deviceCallback is required in the options object");
            callback();
            return;
          }
        }
      });
  apiFunctions.setCustomCallback('startDiscovery',
      function(name, request, response) {
        if (chrome.runtime.lastError) {
          clearDeviceDiscoveredHandler();
          return;
        }
      });
  apiFunctions.setHandleRequest('stopDiscovery',
      function() {
        clearDeviceDiscoveredHandler();
        sendRequest(this.name, arguments, this.definition.parameters);
      });
});
