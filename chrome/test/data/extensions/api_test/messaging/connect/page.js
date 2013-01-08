// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

JSON.parse = function() {
  return "JSON.parse clobbered by content script.";
};

JSON.stringify = function() {
  return "JSON.stringify clobbered by content script.";
};

Array.prototype.toJSON = function() {
  return "Array.prototype.toJSON clobbered by content script.";
};

Object.prototype.toJSON = function() {
  return "Object.prototype.toJSON clobbered by content script.";
};

// For complex connect tests.
chrome.runtime.onConnect.addListener(function(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + msg);
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testPostMessageFromTab) {
      testPostMessageFromTab(port);
    } else if (msg.testSendMessageFromTab) {
      testSendMessageFromTab();
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testDisconnectOnClose) {
      window.location = "about:blank";
    } else if (msg.testPortName) {
      port.postMessage({portName:port.name});
    } else if (msg.testSendMessageFromTabError) {
      testSendMessageFromTabError();
    } else if (msg.testConnectFromTabError) {
      testConnectFromTabError();
    }
  });
});

// Tests that postMessage to the extension and its response works.
function testPostMessageFromTab(origPort) {
  var portName = "peter";
  var port = chrome.runtime.connect({name: portName});
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    origPort.postMessage({success: (msg.success && (msg.portName == portName))});
    console.log('testPostMessageFromTab sent ' + msg.success);
    port.disconnect();
  });
}

// For test onMessage.
function testSendMessageFromTab() {
  chrome.runtime.sendMessage({step: 1}, function(response) {
    if (response.nextStep) {
      console.log('testSendMessageFromTab sent');
      chrome.runtime.sendMessage({step: 2});
    }
  });
}

// Tests sendMessage to an invalid extension.
function testSendMessageFromTabError() {
  // try sending a request to a bad extension id
  chrome.runtime.sendMessage("bad-extension-id", {m: 1}, function(response) {
    var success = (response === undefined && chrome.runtime.lastError);
    chrome.runtime.sendMessage({success: success});
  });
}

// Tests connecting to an invalid extension.
function testConnectFromTabError() {
  var port = chrome.runtime.connect("bad-extension-id");
  port.onDisconnect.addListener(function() {
    var success = (chrome.runtime.lastError ? true : false);
    chrome.runtime.sendMessage({success: success});
  });
}

// For test sendMessage.
chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  sendResponse({success: (request.step2 == 1)});
});
