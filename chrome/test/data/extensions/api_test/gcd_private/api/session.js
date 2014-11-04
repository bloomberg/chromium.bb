// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function session() {
      function onSessionEstablished(sessionId, status, pairingTypes) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq(["embeddedCode"], pairingTypes);
        chrome.gcdPrivate.sendMessage(sessionId, "/privet/ping", {},
                                      onMessageSentFail);
        chrome.gcdPrivate.startPairing(sessionId, "embeddedCode",
                                       onPairingStarted.bind(null, sessionId));
      }

      function onPairingStarted(sessionId, status) {
        chrome.test.assertEq("success", status);
        chrome.gcdPrivate.sendMessage(sessionId, "/privet/ping", {},
                                      onMessageSentFail);
        chrome.gcdPrivate.confirmCode(sessionId, "1234",
                                      onCodeConfirmed.bind(null, sessionId));
      }

      function onCodeConfirmed(sessionId, status) {
        chrome.test.assertEq("success", status);
        chrome.gcdPrivate.sendMessage(sessionId, "/privet/ping", {},
                                      onMessageSent);
      }

      function onMessageSentFail(status, output) {
        chrome.test.assertEq("sessionError", status);
      }

      function onMessageSent(status, output) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq("pong", output.response);
        chrome.test.notifyPass();
      }

      chrome.gcdPrivate.establishSession("1.2.3.4", 9090, onSessionEstablished);
    }
  ]);
};
