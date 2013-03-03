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
chromeHidden.registerCustomHook('bluetooth', function(api) {
  var apiFunctions = api.apiFunctions;

  chromeHidden.bluetooth = {};

  function callCallbackIfPresent(args) {
    if (typeof(args[args.length-1]) == "function") {
      args[args.length-1]();
    }
  }

  chromeHidden.bluetooth.deviceDiscoveredHandler = null;
  chromeHidden.bluetooth.onDeviceDiscovered =
      new chrome.Event("bluetooth.onDeviceDiscovered");
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

  // An object to hold state during one call to getDevices.
  chromeHidden.bluetooth.getDevicesState = null;

  // Hidden events used to deliver getDevices data to the client callbacks
  chromeHidden.bluetooth.onDeviceSearchResult =
      new chrome.Event("bluetooth.onDeviceSearchResult");
  chromeHidden.bluetooth.onDeviceSearchFinished =
      new chrome.Event("bluetooth.onDeviceSearchFinished");

  function deviceSearchResultHandler(device) {
    chromeHidden.bluetooth.getDevicesState.actualEvents++;
    chromeHidden.bluetooth.getDevicesState.deviceCallback(device);
    maybeFinishDeviceSearch();
  }

  function deviceSearchFinishedHandler(info) {
    chromeHidden.bluetooth.getDevicesState.expectedEventCount =
        info.expectedEventCount;
    maybeFinishDeviceSearch();
  }

  function addDeviceSearchListeners() {
    chromeHidden.bluetooth.onDeviceSearchResult.addListener(
        deviceSearchResultHandler);
    chromeHidden.bluetooth.onDeviceSearchFinished.addListener(
        deviceSearchFinishedHandler);
  }

  function removeDeviceSearchListeners() {
    chromeHidden.bluetooth.onDeviceSearchResult.removeListener(
        deviceSearchResultHandler);
    chromeHidden.bluetooth.onDeviceSearchFinished.removeListener(
        deviceSearchFinishedHandler);
  }

  function maybeFinishDeviceSearch() {
    var state = chromeHidden.bluetooth.getDevicesState;
    if (typeof(state.expectedEventCount) != 'undefined' &&
        state.actualEvents >= state.expectedEventCount) {
      finishDeviceSearch();
    }
  }

  function finishDeviceSearch() {
    var finalCallback = chromeHidden.bluetooth.getDevicesState.finalCallback;
    removeDeviceSearchListeners();
    chromeHidden.bluetooth.getDevicesState = null;

    if (finalCallback) {
      finalCallback();
    }
  }

  apiFunctions.setUpdateArgumentsPostValidate('getDevices',
      function() {
        var args = Array.prototype.slice.call(arguments);

        if (chromeHidden.bluetooth.getDevicesState != null) {
          throw new Error("Concurrent calls to getDevices are not allowed.");
        }

        var state = { actualEvents: 0 };

        if (typeof(args[args.length - 1]) == "function") {
          state.finalCallback = args.pop();
          args.push(
              function() {
                if (chrome.runtime.lastError) {
                  finishDeviceSearch();
                }
              });
        } else {
          throw new Error("getDevices must have a final callback parameter.");
        }

        if (typeof(args[0].deviceCallback) == "function") {
          state.deviceCallback = args[0].deviceCallback;
        } else {
          throw new Error("getDevices must be passed options with a " +
              "deviceCallback.");
        }

        chromeHidden.bluetooth.getDevicesState = state;
        addDeviceSearchListeners();

        return args;
      });
});
