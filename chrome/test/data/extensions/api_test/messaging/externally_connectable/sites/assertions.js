// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.assertions = (function() {

// Codes for test results. Must match ExternallyConnectableMessagingTest::Result
// in c/b/extensions/extension_messages_apitest.cc.
var results = {
  OK: 0,
  NAMESPACE_NOT_DEFINED: 1,
  FUNCTION_NOT_DEFINED: 2,
  COULD_NOT_ESTABLISH_CONNECTION_ERROR: 3,
  OTHER_ERROR: 4,
  INCORRECT_RESPONSE_SENDER: 5,
  INCORRECT_RESPONSE_MESSAGE: 6
};

// Make the messages sent vaguely complex, but unambiguously JSON-ifiable.
var message = [{'a': {'b': 10}}, 20, 'c\x10\x11'];

function checkLastError(reply) {
  if (!chrome.runtime.lastError)
    return true;
  var kCouldNotEstablishConnection =
    'Could not establish connection. Receiving end does not exist.';
  if (chrome.runtime.lastError.message == kCouldNotEstablishConnection)
    reply(results.COULD_NOT_ESTABLISH_CONNECTION_ERROR);
  else
    reply(results.OTHER_ERROR);
  return false;
}

function checkResponse(response, reply) {
  // The response will be an echo of both the original message *and* the
  // MessageSender (with the tab field stripped down).
  //
  // First check the sender was correct.
  var incorrectSender = false;
  if (!response.sender.hasOwnProperty('tab')) {
    console.warn('Expected a tab, got none');
    incorrectSender = true;
  }
  if (response.sender.tab.url != document.location.href) {
    console.warn('Expected tab url ' + document.location.href + ' got ' +
                 response.sender.tab.url);
    incorrectSender = true;
  }
  if (response.sender.hasOwnProperty('id')) {
    console.warn('Expected no id, got "' + response.sender.id + '"');
    incorrectSender = true;
  }
  if (response.sender.url != document.location.href) {
    console.warn('Expected url ' + document.location.href + ' got ' +
                 response.sender.url);
    incorrectSender = true;
  }
  if (incorrectSender) {
    reply(results.INCORRECT_RESPONSE_SENDER);
    return false;
  }

  // Check the correct content was echoed.
  var expectedJson = JSON.stringify(message);
  var actualJson = JSON.stringify(response.message);
  if (actualJson == expectedJson)
    return true;
  console.warn('Expected message ' + expectedJson + ' got ' + actualJson);
  reply(results.INCORRECT_RESPONSE_MESSAGE);
  return false;
}

var sendToBrowser = domAutomationController.send.bind(domAutomationController);

return {
  canConnectAndSendMessages: function(extensionId) {
    if (!chrome.runtime) {
      sendToBrowser(results.NAMESPACE_NOT_DEFINED);
      return;
    }

    if (!chrome.runtime.sendMessage || !chrome.runtime.connect) {
      sendToBrowser(results.FUNCTION_NOT_DEFINED);
      return;
    }

    function canSendMessage(reply) {
      chrome.runtime.sendMessage(extensionId, message, function(response) {
        if (checkLastError(reply) && checkResponse(response, reply))
          reply(results.OK);
      });
    }

    function canConnectAndSendMessages(reply) {
      var port = chrome.runtime.connect(extensionId);
      port.postMessage(message, function() {
        checkLastError(reply);
      });
      port.postMessage(message, function() {
        checkLastError(reply);
      });
      var pendingResponses = 2;
      var ok = true;
      port.onMessage.addListener(function(response) {
        pendingResponses--;
        ok = ok && checkLastError(reply) && checkResponse(response, reply);
        if (pendingResponses == 0 && ok)
          reply(results.OK);
      });
    }

    canSendMessage(function(result) {
      if (result != results.OK)
        sendToBrowser(result);
      else
        canConnectAndSendMessages(sendToBrowser);
    });
  },

  areAnyRuntimePropertiesDefined: function(names) {
    var result = false;
    if (chrome.runtime) {
      names.forEach(function(name) {
        if (chrome.runtime[name]) {
          console.log('runtime.' + name + ' is defined');
          result = true;
        }
      });
    }
    sendToBrowser(result);
  }
};

}());  // window.assertions
