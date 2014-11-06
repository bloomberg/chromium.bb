// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CryptoToken background page
 */

'use strict';

/** @const */
var BROWSER_SUPPORTS_TLS_CHANNEL_ID = true;

/** @const */
var LOG_SAVER_EXTENSION_ID = 'fjajfjhkeibgmiggdfehjplbhmfkialk';

// Singleton tracking available devices.
var gnubbies = new Gnubbies();
HidGnubbyDevice.register(gnubbies);
UsbGnubbyDevice.register(gnubbies);

var TIMER_FACTORY = new CountdownTimerFactory();

var FACTORY_REGISTRY = new FactoryRegistry(
    new GoogleApprovedOrigins(),
    TIMER_FACTORY,
    new GstaticOriginChecker(),
    new UsbHelper(),
    new XhrTextFetcher());

var DEVICE_FACTORY_REGISTRY = new DeviceFactoryRegistry(
    new UsbGnubbyFactory(gnubbies),
    TIMER_FACTORY,
    new GoogleCorpIndividualAttestation());

/**
 * Listen to individual messages sent from (whitelisted) webpages via
 * chrome.runtime.sendMessage
 * @param {*} request The received request
 * @param {!MessageSender} sender The message sender
 * @param {function(*): void} sendResponse A callback that delivers a response
 * @return {boolean}
 */
function messageHandlerExternal(request, sender, sendResponse) {
  if (sender.id && sender.id === LOG_SAVER_EXTENSION_ID) {
    return handleLogSaverMessage(request);
  }
  var closeable = handleWebPageRequest(/** @type {Object} */(request),
      sender, function(response) {
    response['requestId'] = request['requestId'];
    try {
      sendResponse(response);
    } catch (e) {
      console.warn(UTIL_fmt('caught: ' + e.message));
    }
  });
  return true;  // Tell Chrome not to destroy sendResponse yet
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

/**
 * Handles messages from the log-saver app. Temporarily replaces UTIL_fmt with
 * a wrapper that also sends formatted messages to the app.
 * @param {*} request The message received from the app
 * @return {boolean} Used as chrome.runtime.onMessage handler return value
 */
function handleLogSaverMessage(request) {
  if (request === 'start') {
    if (originalUtilFmt_) {
      // We're already sending
      return false;
    }
    originalUtilFmt_ = UTIL_fmt;
    UTIL_fmt = function(s) {
      var line = originalUtilFmt_(s);
      chrome.runtime.sendMessage(LOG_SAVER_EXTENSION_ID, line);
      return line;
    };
  } else if (request === 'stop') {
    if (originalUtilFmt_) {
      UTIL_fmt = originalUtilFmt_;
      originalUtilFmt_ = null;
    }
  }
  return false;
}

/** @private */
var originalUtilFmt_ = null;
