// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function session() {
      function onConfirmCode(sessionId, status, code, method) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq("01234", code);
        chrome.gcdPrivate.confirmCode(sessionId,
                                      onSessionEstablished.bind(null,
                                                                sessionId));
      }

      function onSessionEstablished(sessionId, status) {
        chrome.test.assertEq("success", status);

        chrome.gcdPrivate.sendMessage(sessionId, "/privet/ping", {},
                                      onMessageSent);
      }

      function onMessageSent(status, output) {
        chrome.test.assertEq("success", status);
        chrome.test.assertEq("pong", output.response);

        chrome.test.notifyPass();
      }

      chrome.gcdPrivate.establishSession("1.2.3.4", 9090, onConfirmCode);
    }
  ]);
};
