// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function wifiMessage() {
      var messagesNeeded = 3;
      var sessionId = -1;

      function onSessionEstablished(newSessionId) {
        sessionId = newSessionId;
        chrome.gcdPrivate.startPairing(sessionId, "embeddedCode",
                                       onPairingStarted);
      }

      function onPairingStarted(status) {
        chrome.test.assertEq("success", status);
        chrome.gcdPrivate.confirmCode(sessionId, "1234", onCodeConfirmed);
      }

      function onCodeConfirmed(status) {
        chrome.test.assertEq("success", status);
        chrome.gcdPrivate.sendMessage(sessionId, "/privet/v3/setup/start", {
          "wifi" : {
          }
        }, onMessageSent.bind(null, "setupParseError"));

        chrome.gcdPrivate.sendMessage(sessionId, "/privet/v3/setup/start", {
          "wifi" : {
            "passphrase": "Blah"
          }
        }, onMessageSent.bind(null, "setupParseError"));

        chrome.gcdPrivate.sendMessage(sessionId, "/privet/v3/setup/start", {
          "wifi" : {
            "ssid": "Blah"
          }
        }, onMessageSent.bind(null, "wifiPasswordError"));
      }

      function onMessageSent(expected_status, status, output) {
        chrome.test.assertEq(expected_status, status);
        messagesNeeded--;
        console.log("Messages needed " + messagesNeeded);

        if (messagesNeeded == 0) {
          chrome.test.notifyPass();
        }
      }

      chrome.gcdPrivate.establishSession("1.2.3.4", 9090, onSessionEstablished);
    }
  ]);
};
