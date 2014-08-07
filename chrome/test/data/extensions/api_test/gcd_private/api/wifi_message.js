// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function wifiMessage() {
      var messages_needed = 3;
      function onConfirmCode(sessionId, status, code, method) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq("01234", code);
        chrome.gcdPrivate.confirmCode(sessionId,
                                      onSessionEstablished.bind(null,
                                                                sessionId));
      }

      function onSessionEstablished(sessionId, status) {
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
        messages_needed--;
        console.log("Messages needed " + messages_needed);

        if (messages_needed == 0) {
          chrome.test.notifyPass();
        }
      }

      chrome.gcdPrivate.establishSession("1.2.3.4", 9090, onConfirmCode);
    }
  ]);
};
