// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function session() {
      function onSessionEstablished(sessionId, status, pairingTypes) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq(["embeddedCode"], pairingTypes);
        chrome.gcdPrivate.startPairing(1234, "pinCode",
                                       onPairingStarted.bind(null));
      }

      function onPairingStarted(status) {
        chrome.test.assertEq("unknownSessionError", status);
        chrome.gcdPrivate.confirmCode(7567, "1234",
                                      onCodeConfirmed.bind(null));
      }

      function onCodeConfirmed(status) {
        chrome.test.assertEq("unknownSessionError", status);
        chrome.gcdPrivate.sendMessage(555, "/privet/ping", {},
                                      onMessageSent);
      }

      function onMessageSent(status, output) {
        chrome.test.assertEq("unknownSessionError", status);
        chrome.test.notifyPass();
      }

      chrome.gcdPrivate.establishSession("1.2.3.4", 9090, onSessionEstablished);
    }
  ]);
};
