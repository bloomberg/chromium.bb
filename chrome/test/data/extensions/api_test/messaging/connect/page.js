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
chrome.extension.onConnect.addListener(function(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + msg);
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testPostMessageFromTab) {
      testPostMessageFromTab(port);
    } else if (msg.testSendRequestFromTab) {
      testSendRequestFromTab();
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testDisconnectOnClose) {
      window.location = "about:blank";
    } else if (msg.testPortName) {
      port.postMessage({portName:port.name});
    } else if (msg.testSendRequestFromTabError) {
      testSendRequestFromTabError();
    } else if (msg.testConnectFromTabError) {
      testConnectFromTabError();
    }
  });
});

// Tests that postMessage to the extension and its response works.
function testPostMessageFromTab(origPort) {
  var portName = "peter";
  var port = chrome.extension.connect({name: portName});
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    origPort.postMessage({success: (msg.success && (msg.portName == portName))});
    console.log('testPostMessageFromTab sent ' + msg.success);
    port.disconnect();
  });
}

// For test onRequest.
function testSendRequestFromTab() {
  chrome.extension.sendRequest({step: 1}, function(response) {
    if (response.nextStep) {
      console.log('testSendRequestFromTab sent');
      chrome.extension.sendRequest({step: 2});
    }
  });
}

// Tests sendRequest to an invalid extension.
function testSendRequestFromTabError() {
  // try sending a request to a bad extension id
  chrome.extension.sendRequest("bad-extension-id", {m: 1}, function(response) {
    var success = (response === undefined && chrome.extension.lastError);
    chrome.extension.sendRequest({success: success});
  });
}

// Tests connecting to an invalid extension.
function testConnectFromTabError() {
  var port = chrome.extension.connect("bad-extension-id");
  port.onDisconnect.addListener(function() {
    var success = (chrome.extension.lastError ? true : false);
    chrome.extension.sendRequest({success: success});
  });
}

// For test sendRequest.
chrome.extension.onRequest.addListener(function(request, sender, sendResponse) {
  sendResponse({success: (request.step2 == 1)});
});
