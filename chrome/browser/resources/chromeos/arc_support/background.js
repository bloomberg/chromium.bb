// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Chrome window that hosts UI. Only one window is allowed.
 * @type {chrome.app.window.AppWindow}
 */
var appWindow = null;

/**
 * Contains Web content provided by Google authorization server.
 * @type {WebView}
 */
var webview = null;

/**
 * Used for bidirectional communication with native code.
 * @type {chrome.runtime.Port}
 */
var port = null;

/**
 * Indicates that window was closed internally and it is not required to send
 * closure notification.
 * @type {boolean}
 */
var windowClosedInternally = false;

/**
 * Sends a native message to ArcSupportHost.
 * @param {string} code The action code in message.
 */
function sendNativeMessage(code) {
  message = {'action': code};
  port.postMessage(message);
}

/**
 * Handles native messages received from ArcSupportHost.
 * @param {!Object} message The message received.
 */
function onNativeMessage(message) {
  if (!message.action) {
    return;
  }

  if (message.action == 'closeUI') {
    if (appWindow) {
      windowClosedInternally = true;
      appWindow.close();
      appWindow = null;
    }
  }
}

/**
 * Connects to ArcSupportHost.
 */
function connectPort() {
  var hostName = 'com.google.arc_support';
  port = chrome.runtime.connectNative(hostName);
  port.onMessage.addListener(onNativeMessage);
}

chrome.app.runtime.onLaunched.addListener(function() {
  var onAppContentLoad = function() {
    var doc = appWindow.contentWindow.document;
    webview = doc.getElementById('arc_support');

    var onWebviewRequestCompleted = function(details) {
      if (details.statusCode == 200) {
        sendNativeMessage('fetchAuthCode');
      }
    };

    var requestFilter = {
      urls: ['https://accounts.google.com/o/oauth2/programmatic_auth?*'],
      types: ['main_frame']
    };

    webview.request.onCompleted.addListener(onWebviewRequestCompleted,
                                            requestFilter);
  };

  var onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    createdWindow.onClosed.addListener(onWindowClosed);
    connectPort();
  };

  var onWindowClosed = function() {
    if (windowClosedInternally) {
      return;
    }
    sendNativeMessage('cancelAuthCode');
  };

  windowClosedInternally = false;
  var options = {
    'id': 'arc_support_wnd',
    'innerBounds': {
      'width': 500,
      'height': 600
    }
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});
