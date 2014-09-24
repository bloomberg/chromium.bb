// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CryptoToken background page
 */

'use strict';

// Singleton tracking available devices.
var gnubbies = new Gnubbies();
HidGnubbyDevice.register(gnubbies);
UsbGnubbyDevice.register(gnubbies);

var TIMER_FACTORY = new CountdownTimerFactory();

var FACTORY_REGISTRY = new FactoryRegistry(
    TIMER_FACTORY,
    new GstaticOriginChecker(),
    new UsbHelper(),
    new XhrTextFetcher());

var DEVICE_FACTORY_REGISTRY = new DeviceFactoryRegistry(
    new UsbGnubbyFactory(gnubbies),
    TIMER_FACTORY,
    new GoogleCorpIndividualAttestation());

// Listen to individual messages sent from (whitelisted) webpages via
// chrome.runtime.sendMessage
function messageHandlerExternal(request, sender, sendResponse) {
  var closeable = handleWebPageRequest(request, sender, function(response) {
    response['requestId'] = request['requestId'];
    try {
      sendResponse(response);
    } catch (e) {
      console.warn(UTIL_fmt('caught: ' + e.message));
    }
  });
}
chrome.runtime.onMessageExternal.addListener(messageHandlerExternal);

// Listen to direct connection events, and wire up a message handler on the port
chrome.runtime.onConnectExternal.addListener(function(port) {
  var closeable;
  port.onMessage.addListener(function(request) {
    closeable = handleWebPageRequest(request, port.sender,
        function(response) {
          response['requestId'] = request['requestId'];
          port.postMessage(response);
        });
  });
  port.onDisconnect.addListener(function() {
    if (closeable) {
      closeable.close();
    }
  });
});
