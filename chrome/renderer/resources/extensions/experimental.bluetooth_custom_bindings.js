// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Bluetooth API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var lastError = require('lastError');

// Use custom bindings to create an undocumented event listener that will
// receive events about device discovery and call the event listener that was
// provided with the request to begin discovery.
chromeHidden.registerCustomHook('experimental.bluetooth', function(api) {
  var apiFunctions = api.apiFunctions;

  chromeHidden.bluetooth = {};

  function callCallbackIfPresent(args) {
    if (typeof(args[args.length-1]) == "function") {
      args[args.length-1]();
    }
  }

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
          if (chromeHidden.bluetooth.deviceDiscoveredHandler != null) {
            lastError.set("Concurrent discovery is not allowed.");
            callCallbackIfPresent(args);
            return;
          }

          chromeHidden.bluetooth.deviceDiscoveredHandler =
              args[0].deviceCallback;
          chromeHidden.bluetooth.onDeviceDiscovered.addListener(
              chromeHidden.bluetooth.deviceDiscoveredHandler);
          sendRequest(this.name,
                      args,
                      this.definition.parameters,
                      {customCallback:this.customCallback});
        } else {
          lastError.set("deviceCallback is required in the options object");
          callCallbackIfPresent(args);
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

  chromeHidden.bluetooth.deviceSearchResultHandler = null;
  chromeHidden.bluetooth.onDeviceSearchResult =
      new chrome.Event("experimental.bluetooth.onDeviceSearchResult");
  apiFunctions.setHandleRequest('getDevices',
      function() {
        var args = arguments;
        if (args.length > 0 && args[0] && args[0].deviceCallback) {
          if (chromeHidden.bluetooth.deviceSearchResultHandler != null) {
            lastError.set("Concurrent calls to getDevices are not allowed.");
            callCallbackIfPresent(args);
            return;
          }

          chromeHidden.bluetooth.deviceSearchResultHandler =
              args[0].deviceCallback;
          chromeHidden.bluetooth.onDeviceSearchResult.addListener(
              chromeHidden.bluetooth.deviceSearchResultHandler);
          sendRequest(this.name,
                      args,
                      this.definition.parameters,
                      {customCallback:this.customCallback});
        } else {
          lastError.set("deviceCallback is required in the options object");
          callCallbackIfPresent(args);
        }
      });
  apiFunctions.setCustomCallback('getDevices',
      function(name, request, response) {
        chromeHidden.bluetooth.onDeviceSearchResult.removeListener(
            chromeHidden.bluetooth.deviceSearchResultHandler);
        chromeHidden.bluetooth.deviceSearchResultHandler = null;
      });
});
