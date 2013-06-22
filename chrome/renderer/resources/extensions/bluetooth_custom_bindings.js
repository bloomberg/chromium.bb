// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Bluetooth API.

var binding = require('binding').Binding.create('bluetooth');

var chrome = requireNative('chrome').GetChrome();
var Event = require('event_bindings').Event;
var lastError = require('lastError');
var sendRequest = require('sendRequest').sendRequest;

// Use custom binding to create an undocumented event listener that will
// receive events about device discovery and call the event listener that was
// provided with the request to begin discovery.
binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  var bluetooth = {};

  function callCallbackIfPresent(name, args, error) {
    var callback = args[args.length - 1];
    if (typeof(callback) == 'function')
      lastError.run(name, error, callback);
  }

  bluetooth.deviceDiscoveredHandler = null;
  bluetooth.onDeviceDiscovered = new Event('bluetooth.onDeviceDiscovered');
  function clearDeviceDiscoveredHandler() {
    bluetooth.onDeviceDiscovered.removeListener(
        bluetooth.deviceDiscoveredHandler);
    bluetooth.deviceDiscoveredHandler = null;
  }
  apiFunctions.setHandleRequest('startDiscovery',
      function() {
        var args = arguments;
        if (args.length > 0 && args[0] && args[0].deviceCallback) {
          if (bluetooth.deviceDiscoveredHandler != null) {
            callCallbackIfPresent('bluetooth.startDiscovery',
                                  args,
                                  'Concurrent discovery is not allowed.');
            return;
          }

          bluetooth.deviceDiscoveredHandler = args[0].deviceCallback;
          bluetooth.onDeviceDiscovered.addListener(
              bluetooth.deviceDiscoveredHandler);
          sendRequest(this.name,
                      args,
                      this.definition.parameters,
                      {customCallback:this.customCallback});
        } else {
          callCallbackIfPresent(
            'bluetooth.startDiscovery',
            args,
            'deviceCallback is required in the options object');
          return;
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
  bluetooth.getDevicesState = null;

  // Hidden events used to deliver getDevices data to the client callbacks
  bluetooth.onDeviceSearchResult = new Event('bluetooth.onDeviceSearchResult');
  bluetooth.onDeviceSearchFinished =
      new Event('bluetooth.onDeviceSearchFinished');

  function deviceSearchResultHandler(device) {
    bluetooth.getDevicesState.actualEvents++;
    bluetooth.getDevicesState.deviceCallback(device);
    maybeFinishDeviceSearch();
  }

  function deviceSearchFinishedHandler(info) {
    bluetooth.getDevicesState.expectedEventCount = info.expectedEventCount;
    maybeFinishDeviceSearch();
  }

  function addDeviceSearchListeners() {
    bluetooth.onDeviceSearchResult.addListener(deviceSearchResultHandler);
    bluetooth.onDeviceSearchFinished.addListener(deviceSearchFinishedHandler);
  }

  function removeDeviceSearchListeners() {
    bluetooth.onDeviceSearchResult.removeListener(deviceSearchResultHandler);
    bluetooth.onDeviceSearchFinished.removeListener(
        deviceSearchFinishedHandler);
  }

  function maybeFinishDeviceSearch() {
    var state = bluetooth.getDevicesState;
    if (typeof(state.expectedEventCount) != 'undefined' &&
        state.actualEvents >= state.expectedEventCount) {
      finishDeviceSearch();
    }
  }

  function finishDeviceSearch() {
    var finalCallback = bluetooth.getDevicesState.finalCallback;
    removeDeviceSearchListeners();
    bluetooth.getDevicesState = null;

    if (finalCallback) {
      finalCallback();
    }
  }

  apiFunctions.setUpdateArgumentsPostValidate('getDevices',
      function() {
        var args = $Array.slice(arguments);

        if (bluetooth.getDevicesState != null) {
          throw new Error('Concurrent calls to getDevices are not allowed.');
        }

        var state = { actualEvents: 0 };

        if (typeof(args[args.length - 1]) == 'function') {
          state.finalCallback = args.pop();
          $Array.push(args,
              function() {
                if (chrome.runtime.lastError) {
                  finishDeviceSearch();
                }
              });
        } else {
          throw new Error('getDevices must have a final callback parameter.');
        }

        if (typeof(args[0].deviceCallback) == 'function') {
          state.deviceCallback = args[0].deviceCallback;
        } else {
          throw new Error('getDevices must be passed options with a ' +
              'deviceCallback.');
        }

        bluetooth.getDevicesState = state;
        addDeviceSearchListeners();

        return args;
      });
});

exports.binding = binding.generate();
